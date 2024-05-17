#include "framework.h"
#include "GrUI.h"
#include <commdlg.h>
#include <fstream>
#include <vector>


#define ID_BUTTON_SELECT_FILE 101
#define ID_BUTTON_SEND_FILE 102

wchar_t selectedFilePath[MAX_PATH];

void SelectFile(HWND hWnd);
void SendFile(HWND hWnd, const std::wstring& filePath);

UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

// {51787A2F-D87A-4E96-99BC-961F6E5B6292}
static const GUID iconGuid =
{ 0x51787a2f, 0xd87a, 0x4e96, { 0x99, 0xbc, 0x96, 0x1f, 0x6e, 0x5b, 0x62, 0x92 } };

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL AddNotificationIcon(HWND hwnd);

bool Read(HANDLE handle, uint8_t* data, uint64_t length, DWORD& bytesRead)
{
    bytesRead = 0;
    BOOL fSuccess = ReadFile(
        handle,
        data,
        length,
        &bytesRead,
        NULL);
    if (!fSuccess || bytesRead == 0)
    {
        return false;
    }
    return true;
}

bool Write(HANDLE handle, uint8_t* data, uint64_t length)
{
    DWORD cbWritten = 0;
    BOOL fSuccess = WriteFile(
        handle,
        data,
        length,
        &cbWritten,
        NULL);
    if (!fSuccess || length != cbWritten)
    {
        return false;
    }
    return true;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GRUI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GRUI));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}



// Регистрирует класс окна.
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GRUI));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_GRUI);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

HANDLE ConnectToServerPipe(const std::wstring& name, uint32_t timeout)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    while (true)
    {
        hPipe = CreateFileW(
            reinterpret_cast<LPCWSTR>(name.c_str()),
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            break;
        }
        DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY)
        {
            return INVALID_HANDLE_VALUE;
        }
        if (!WaitNamedPipe(reinterpret_cast<LPCWSTR>(name.c_str()), timeout))
        {
            return INVALID_HANDLE_VALUE;
        }
    }
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(
        hPipe,
        &dwMode,
        NULL,
        NULL);
    if (!fSuccess)
    {
        return INVALID_HANDLE_VALUE;
    }
    return hPipe;
}


//  Сохраняет маркер экземпляра и создает главное окно
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }
    // проверка task bar
    AddNotificationIcon(hWnd);
    return TRUE;
}

BOOL AddNotificationIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = iconGuid;
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_GRUI));
    LoadString(hInst, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
    Shell_NotifyIcon(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

BOOL DeleteNotificationIcon()
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_GUID;
    nid.guidItem = iconGuid;
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
    HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU1));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            SetForegroundWindow(hwnd);
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }

            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

// Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static UINT s_uTaskbarRestart;
    switch (message)
    {
    case WM_CREATE:
        s_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));

        CreateWindowW(L"BUTTON", L"Select File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 10, 120, 30, hWnd, (HMENU)ID_BUTTON_SELECT_FILE, hInst, nullptr);

        CreateWindowW(L"BUTTON", L"Send File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            140, 10, 120, 30, hWnd, (HMENU)ID_BUTTON_SEND_FILE, hInst, nullptr);
        break;
        break;
    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case NIN_SELECT:
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);
            break;
        case WM_CONTEXTMENU:
        {
            POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
            ShowContextMenu(hWnd, pt);
        }
        break;
        }

        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_CONTEXTMENU_SHOW_MAIN_WINDOW:
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);
            break;
        case ID_BUTTON_SELECT_FILE:
            SelectFile(hWnd);
            break;
        case ID_BUTTON_SEND_FILE:
            SendFile(hWnd, selectedFilePath);
            break;
        case ID_CONTEXTMENU_EXIT:
        case IDM_EXIT:
            if (IDYES == MessageBox(hWnd, L"Are you shure to close window?", L"WM_CONTEXTMENU", MB_ICONWARNING | MB_YESNO)) {
                DestroyWindow(hWnd);
            }
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_CLOSE:
        //DestroyWindow(hWnd);
        ShowWindow(hWnd, SW_HIDE);
        UpdateWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        if (message == s_uTaskbarRestart)
            AddNotificationIcon(hWnd);
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


void SelectFile(HWND hWnd)
{
    OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        wcscpy_s(selectedFilePath, MAX_PATH, szFile);
    }
}

void SendFile(HWND hWnd, const std::wstring& filePath)
{
    // Открываем pipe для записи
    std::thread clientThread([hWnd, filePath]() {
        DWORD sessionId;
        ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
        std::wstring path = std::format(L"\\\\.\\pipe\\MyService_{}", sessionId);
        HANDLE pipe = ConnectToServerPipe(path, 0);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            MessageBox(nullptr, L"Failed to connect to the pipe.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Открываем файл для чтения
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            MessageBox(nullptr, L"Failed to open the file.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Читаем файл и отправляем его данные по pipe
        const size_t bufferSize = 1024; // Размер буфера для чтения из файла
        DWORD bytesRead;
        DWORD bytesWritten;

        // Получаем размер файла
        file.seekg(0, std::ios::end);
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Создаем буфер для чтения файла
        std::vector<char> buffer(fileSize);

        // Читаем содержимое файла в буфер
        if (!file.read(buffer.data(), fileSize))
        {
            MessageBox(nullptr, L"Failed to read from the file.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Записываем содержимое файла в pipe
        if (!Write(pipe, reinterpret_cast<uint8_t*>(buffer.data()), buffer.size()))
        {
            MessageBox(nullptr, L"Failed to write to the pipe.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        file.close();
        CloseHandle(pipe);

        MessageBox(nullptr, L"File sent successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
        });
    clientThread.detach();

}
