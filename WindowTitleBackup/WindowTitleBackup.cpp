#include <windows.h>
#include <gdiplus.h>
#include <ShObjIdl.h>
#include <ShObjIdl_core.h>
#include <cassert>
#include <ctime>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
//#include "resource.h"
using namespace Gdiplus;
using namespace std;
#pragma comment(lib, "gdiplus")
#pragma warning(disable: 4996)

const int WIDTH = 300;
const int HEIGHT = 50;


IVirtualDesktopManager *vdm;

HINSTANCE g_hInst;
HWND hWndMain;
LPCTSTR lpszClass = TEXT("GdiPlusStart");

struct comp
{
	bool operator() (const GUID a, const GUID b) const
	{
		if(a.Data1 != b.Data1) return a.Data1 < b.Data1;
		if(a.Data2 != b.Data2) return a.Data2 < b.Data2;
		if(a.Data3 != b.Data3) return a.Data3 < b.Data3;
		for(int i = 0; i < 8; i++) {
			if(a.Data4[i] != b.Data4[i]) return a.Data4[i] < b.Data4[i];
		}
		return a.Data4[7] < b.Data4[7];
	}
};
static map<GUID, vector<pair<LPWSTR, RECT>>, comp> GUID2title;

void OnPaint(HDC hdc, int ID, int x, int y);
void OnPaintA(HDC hdc, int ID, int x, int y, double alpha);
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	HWND     hWnd;
	MSG		 msg;
	WNDCLASS WndClass;

	g_hInst = hInstance;

	ULONG_PTR gpToken;
	GdiplusStartupInput gpsi;
	if(GdiplusStartup(&gpToken, &gpsi, NULL) != Ok) {
		MessageBox(NULL, TEXT("GDI+ 라이브러리를 초기화할 수 없습니다."), TEXT("알림"), MB_OK);
		return 0;
	}


	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = L"WindowTitleBackup";
	RegisterClass(&WndClass);
	hWnd = CreateWindow(
		L"WindowTitleBackup", // Window Class Name
		L"WindowTitleBackup", //Window Title Name
		WS_OVERLAPPEDWINDOW,
		GetSystemMetrics(SM_CXFULLSCREEN) / 2 - WIDTH / 2,
		GetSystemMetrics(SM_CYFULLSCREEN) / 2 - HEIGHT / 2,
		WIDTH,
		HEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}


BOOL /*CALLBACK*/ EnumWindowsProc(HWND hWnd, LPARAM lparam)
{
	WCHAR buf[1025] = L"";
	int len = GetWindowTextW(hWnd, buf, 1024);
	if(len == 0) return TRUE;
	if(!IsWindowVisible(hWnd)) return TRUE;

	auto vdm = ((pair<IVirtualDesktopManager*, vector<tuple<GUID, LPWSTR, RECT>>*>*)lparam)->first;
	auto v = ((pair<IVirtualDesktopManager*, vector<tuple<GUID, LPWSTR, RECT>>*>*)lparam)->second;

	if(vdm != NULL) {
		BOOL isCurrent;
		if(!(vdm && SUCCEEDED(vdm->IsWindowOnCurrentVirtualDesktop(hWnd, &isCurrent)) && isCurrent)) {
			return TRUE;
		}
	}
	//else return FALSE;

	WCHAR *p = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
	wcscpy(p, buf);
	RECT rt;
	GetWindowRect(hWnd, &rt);
	
	auto ret = CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&vdm));
	assert(TRUE);
	assert(vdm != NULL);
	GUID bottommost;
	vdm->GetWindowDesktopId(hWnd, &bottommost);

	if(GUID2title.count(bottommost) == 0) {
		vector<pair<LPWSTR, RECT>> v;
		v.push_back(make_pair(p, rt));
		GUID2title[bottommost] = v;
	}
	else {
		GUID2title[bottommost].push_back(make_pair(p, rt));
	}

	v->push_back(make_tuple(bottommost, p, rt));
	return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc, MemDC;
	PAINTSTRUCT ps;

	HBITMAP hBit, OldBit;
	RECT crt;


	switch(iMsg) {
		case WM_CREATE:
		{
			SetTimer(hWnd, 1, 30000, 0);

			break;
		}

		case WM_TIMER:
		{
			InvalidateRect(hWnd, NULL, FALSE);

			vector<tuple<GUID, LPWSTR, RECT>> v;
			pair<IVirtualDesktopManager*, vector<tuple<GUID, LPWSTR, RECT>>*> p = make_pair(vdm, &v);
			//EnumWindows(EnumWindowsProc, (LPARAM)&p);
			HWND window = GetTopWindow(GetDesktopWindow());
			while(window != NULL) {
				if(window != hWnd) {
					EnumWindowsProc(window, (LPARAM)&p);
				}
				window = GetWindow(window, GW_HWNDNEXT);
			}
			//vdm->Release();

			WCHAR filename[100] = L"";
			wsprintf(filename, L"open_window_list_%d.txt", time(NULL));
			FILE *fp = _wfopen(filename, L"w, ccs=UTF-8");
			for(auto desktop : GUID2title) {
				auto guid = desktop.first;
				auto v = desktop.second;
				fwprintf(fp, L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
					guid.Data1, guid.Data2, guid.Data3,
					guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
					guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
				);
				for(auto x : v) {
					LPWSTR window_name = x.first;
					RECT rect = x.second;
					fwprintf(fp, L"%s (%d %d %d %d)\n", window_name, rect.top, rect.left, rect.right, rect.bottom);
				}
				fwprintf(fp, L"\n");
			}
			fclose(fp);

			GUID2title.clear();
			break;
		}

		case WM_PAINT:
		{
			hdc = BeginPaint(hWnd, &ps);
			GetClientRect(hWnd, &crt);

			MemDC = CreateCompatibleDC(hdc);
			hBit = CreateCompatibleBitmap(hdc, crt.right, crt.bottom);
			OldBit = (HBITMAP)SelectObject(MemDC, hBit);
			//hBrush = CreateSolidBrush(RGB(255, 255, 255));
			//oldBrush = (HBRUSH)SelectObject(MemDC, hBrush);
			//hPen = CreatePen(PS_SOLID, 5, RGB(255, 255, 255));
			//oldPen = (HPEN)SelectObject(MemDC, hPen);

			//FillRect(MemDC, &crt, hBrush);
			SetBkColor(MemDC, RGB(255, 255, 255));



			//OnPaint(MemDC, TITLE0, 0, 0);

			BitBlt(hdc, 0, 0, crt.right, crt.bottom, MemDC, 0, 0, SRCCOPY);
			SelectObject(MemDC, OldBit);
			DeleteDC(MemDC);
			//SelectObject(MemDC, oldPen);
			//DeleteObject(hPen);
			//SelectObject(MemDC, oldBrush);
			//DeleteObject(hBrush);
			DeleteObject(hBit);

			EndPaint(hWnd, &ps);
			break;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

void OnPaint(HDC hdc, int ID, int x, int y)
{
	Graphics G(hdc);
	HRSRC hResource = FindResource(g_hInst, MAKEINTRESOURCE(ID), TEXT("PNG"));
	if(!hResource)
		return;

	DWORD imageSize = SizeofResource(g_hInst, hResource);
	HGLOBAL hGlobal = LoadResource(g_hInst, hResource);
	LPVOID pData = LockResource(hGlobal);

	HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
	LPVOID pBuffer = GlobalLock(hBuffer);
	CopyMemory(pBuffer, pData, imageSize);
	GlobalUnlock(hBuffer);

	IStream *pStream;
	HRESULT hr = CreateStreamOnHGlobal(hBuffer, TRUE, &pStream);

	Image I(pStream);
	pStream->Release();
	if(I.GetLastStatus() != Ok) return;

	G.DrawImage(&I, x, y, I.GetWidth(), I.GetHeight());
}


void OnPaintA(HDC hdc, int ID, int x, int y, double alpha)
{
	Graphics G(hdc);
	HRSRC hResource = FindResource(g_hInst, MAKEINTRESOURCE(ID), TEXT("PNG"));
	if(!hResource)
		return;

	ColorMatrix ClrMatrix =
	{
		1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, (Gdiplus::REAL)alpha, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 1.0f
	};

	ImageAttributes ImgAttr;
	ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

	DWORD imageSize = SizeofResource(g_hInst, hResource);
	HGLOBAL hGlobal = LoadResource(g_hInst, hResource);
	LPVOID pData = LockResource(hGlobal);

	HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
	LPVOID pBuffer = GlobalLock(hBuffer);
	CopyMemory(pBuffer, pData, imageSize);
	GlobalUnlock(hBuffer);

	IStream *pStream;
	HRESULT hr = CreateStreamOnHGlobal(hBuffer, TRUE, &pStream);

	Image I(pStream);
	pStream->Release();
	if(I.GetLastStatus() != Ok) return;

	RectF destination(0, 0, (Gdiplus::REAL)I.GetWidth(), (Gdiplus::REAL)I.GetHeight());
	G.DrawImage(&I, destination, x, y, (Gdiplus::REAL)I.GetWidth(), (Gdiplus::REAL)I.GetHeight(), UnitPixel, &ImgAttr);
}