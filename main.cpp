#include <windows.h>
#include "main.h"
#include <tchar.h>

// 按钮ID
#define BTN_OPEN    1001
#define BTN_SAVE    1002
#define BTN_EXIT    1003

// 自动识别编码读取文件（不乱码）
bool CheckUTF8String(const char* pData, int nLen)
{
	int nBytes = 0;
	for (int i = 0; i < nLen; i++)
	{
		BYTE c = (BYTE)pData[i];
		if (c == 0) return true;
		if (nBytes == 0)
		{
			if ((c >> 7) == 0)      continue;
			else if ((c >> 5) == 0x06) nBytes = 1;
			else if ((c >> 4) == 0x0E) nBytes = 2;
			else if ((c >> 3) == 0x1E) nBytes = 3;
			else return false;
		}
		else
		{
			if ((c >> 6) != 0x02) return false;
			nBytes--;
		}
	}
	return true;
}

BOOL LoadFile(HWND hEdit, LPCSTR pszFileName) {
	HANDLE hFile = CreateFileA(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == 0xFFFFFFFF) { CloseHandle(hFile); return FALSE; }
	
	char* buffer = (char*)GlobalAlloc(GPTR, fileSize + 2);
	if (!buffer) { CloseHandle(hFile); return FALSE; }
	
	DWORD readLen;
	ReadFile(hFile, buffer, fileSize, &readLen, NULL);
	CloseHandle(hFile);
	
	wchar_t* wideStr = NULL;
	
	if (fileSize >= 3 && (BYTE)buffer[0] == 0xEF && (BYTE)buffer[1] == 0xBB && (BYTE)buffer[2] == 0xBF)
	{
		int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer + 3, -1, NULL, 0);
		wideStr = (wchar_t*)GlobalAlloc(GPTR, wlen * 2);
		MultiByteToWideChar(CP_UTF8, 0, buffer + 3, -1, wideStr, wlen);
	}
	else if (fileSize >= 2 && (BYTE)buffer[0] == 0xFF && (BYTE)buffer[1] == 0xFE)
	{
		wideStr = (wchar_t*)GlobalAlloc(GPTR, fileSize);
		memcpy(wideStr, buffer + 2, fileSize - 2);
	}
	else if (CheckUTF8String(buffer, readLen))
	{
		int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
		wideStr = (wchar_t*)GlobalAlloc(GPTR, wlen * 2);
		MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wideStr, wlen);
	}
	else
	{
		int wlen = MultiByteToWideChar(CP_ACP, 0, buffer, -1, NULL, 0);
		wideStr = (wchar_t*)GlobalAlloc(GPTR, wlen * 2);
		MultiByteToWideChar(CP_ACP, 0, buffer, -1, wideStr, wlen);
	}
	
	GlobalFree(buffer);
	BOOL bSuccess = SetWindowTextW(hEdit, wideStr);
	GlobalFree(wideStr);
	return bSuccess;
}

BOOL SaveFile(HWND hEdit, LPCSTR pszFileName) {
	int len = GetWindowTextLengthW(hEdit);
	if (len <= 0) return FALSE;
	
	wchar_t* wideText = (wchar_t*)GlobalAlloc(GPTR, (len + 2) * sizeof(wchar_t));
	GetWindowTextW(hEdit, wideText, len + 1);
	
	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideText, -1, NULL, 0, NULL, NULL);
	char* utf8Buf = (char*)GlobalAlloc(GPTR, utf8Len);
	WideCharToMultiByte(CP_UTF8, 0, wideText, -1, utf8Buf, utf8Len, NULL, NULL);
	
	HANDLE hFile = CreateFileA(pszFileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	BOOL bSuccess = FALSE;
	
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD written;
		if (WriteFile(hFile, utf8Buf, utf8Len - 1, &written, NULL)) {
			bSuccess = TRUE;
		}
		CloseHandle(hFile);
	}
	
	GlobalFree(wideText);
	GlobalFree(utf8Buf);
	return bSuccess;
}

BOOL DoFileOpenSave(HWND hwnd, BOOL bSave) {
	OPENFILENAMEA ofn;
	char szFileName[MAX_PATH];
	ZeroMemory(&ofn, sizeof(ofn));
	szFileName[0] = 0;
	
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "txt";
	
	if (bSave) {
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		if (GetSaveFileNameA(&ofn)) {
			if (!SaveFile(GetDlgItem(hwnd, IDC_MAIN_TEXT), szFileName)) {
				MessageBoxA(hwnd, "保存失败", "Error", MB_OK | MB_ICONEXCLAMATION);
				return FALSE;
			}
		}
	}
	else {
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		if (GetOpenFileNameA(&ofn)) {
			if (!LoadFile(GetDlgItem(hwnd, IDC_MAIN_TEXT), szFileName)) {
				MessageBoxA(hwnd, "打开失败", "Error", MB_OK | MB_ICONEXCLAMATION);
				return FALSE;
			}
		}
	}
	return TRUE;
}

// ==============================
// 窗口过程（修复无限弹窗）
// ==============================
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	switch (Message) {
	case WM_CREATE:
		// 创建文本框
		CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
					  0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
					  hwnd, (HMENU)IDC_MAIN_TEXT, GetModuleHandle(NULL), NULL);
		SendDlgItemMessage(hwnd, IDC_MAIN_TEXT, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
		break;
		
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
			MoveWindow(GetDlgItem(hwnd, IDC_MAIN_TEXT), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		break;
		
	case WM_SETFOCUS:
		SetFocus(GetDlgItem(hwnd, IDC_MAIN_TEXT));
		break;
		
	case WM_COMMAND:
		// 只处理菜单/按钮，修复死循环
		if(HIWORD(wParam) == 0 || HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD(wParam)) {
			case CM_FILE_OPEN:
				DoFileOpenSave(hwnd, FALSE);
				break;
			case CM_FILE_SAVEAS:
				DoFileOpenSave(hwnd, TRUE);
				break;
			case CM_FILE_EXIT:
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case CM_ABOUT:
				MessageBoxA(NULL, "文件编辑器", "关于", 0);
				break;
			}
		}
		break;
		
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
		
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
		
	default:
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

// ==============================
// 主函数
// ==============================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;
	
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = "MAINMENU";
	wc.lpszClassName = "WindowClass";
	wc.hIconSm       = LoadIcon(hInstance, "A");
	
	if(!RegisterClassEx(&wc)) {
		MessageBoxA(0, "窗口注册失败!", "错误", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "WindowClass", "我的文件编辑器",
						   WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
						   NULL, NULL, hInstance, NULL);
	
	if(hwnd == NULL) {
		MessageBoxA(0, "窗口创建失败!", "错误", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	ShowWindow(hwnd, SW_MAXIMIZE);
	UpdateWindow(hwnd);
	
	while(GetMessage(&Msg, NULL, 0, 0) > 0) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}
