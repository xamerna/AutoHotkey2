#include "stdafx.h"
#include "globaldata.h"
#include "autogenerated/ahklib_h.h"

ULONG s_modRef = 0;
STDAPI DllCanUnloadNow()
{
	if (s_modRef > 0)
		return S_FALSE;
	s_modRef = 0;
	return S_OK;
}


HRESULT CreateAutoHotkeyLibInstance(void **ppInst);
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID *ppv)
{
	class ClassFactory : public IClassFactory
	{
	public:
		ClassFactory() {}
		~ClassFactory() {}
		virtual HRESULT _stdcall QueryInterface(const IID &riid, void **ppvObject) {
			if (riid == IID_IUnknown || riid == IID_IClassFactory)
			{
				*ppvObject = this;
				return S_OK;
			}
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
		virtual ULONG _stdcall AddRef() { return 1; }
		virtual ULONG _stdcall Release() { return 1; }
		virtual HRESULT _stdcall CreateInstance(IUnknown *pUnknown, const IID &riid, void **ppvObject) {
			if (pUnknown)
				return CLASS_E_NOAGGREGATION;
			if (!ppvObject)
				return E_INVALIDARG;
			*ppvObject = nullptr;
			if (riid == IID_IDispatch || riid == IID_IAutoHotkeyLib)
				return CreateAutoHotkeyLibInstance(ppvObject);
			return E_NOINTERFACE;
		}
		virtual HRESULT _stdcall LockServer(BOOL fLock) {
			fLock ? InterlockedIncrement(&s_modRef) : InterlockedDecrement(&s_modRef);
			return S_OK;
		}
	};
	static ClassFactory sCF;
	if (rclsid == CLSID_AutoHotkey)
		return sCF.QueryInterface(riid, ppv);
	return CLASS_E_CLASSNOTAVAILABLE;
}


STDAPI DllInstall(BOOL bInstall, _In_opt_  LPCWSTR pszCmdLine)
{
	auto progID = _T("AutoHotkey2.Script");
	auto desc = _T("AutoHotkey v2");
	auto version = _T("1.0");
	auto &clsid = CLSID_AutoHotkey;
	auto &typelib = LIBID_ComLib;

	int hr;
	HKEY hkey = HKEY_CLASSES_ROOT;
	TCHAR subkey[MAX_PATH] = _T("CLSID"), *op = subkey + 5;

	if (pszCmdLine && _wcsnicmp(pszCmdLine, L"user", 5) == 0)
	{
		if (hr = RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Classes"), &hkey))
			return ((HRESULT)(((hr) & 0x0000FFFF) | 0x80070000));
	}

	static auto writeGUID = [](const GUID &guid, LPTSTR buf) {
#ifdef _UNICODE
		return StringFromGUID2(guid, buf, 40) - 1;
#else
		WCHAR wbuf[64];
		int len = StringFromGUID2(guid, wbuf, 40);
		for (int i = 0; i < len; i++)
			buf[i] = wbuf[i];
		return len - 1;
#endif // _UNICODE
	};

	*op = '\\';
	op += writeGUID(clsid, ++op);
	*op++ = '\\', *op = 0;

	if (bInstall)
	{
		TCHAR szModule[MAX_PATH + 1], buf[40];
		if (!GetModuleFileName(g_hInstance, szModule, MAX_PATH + 1))
			return HRESULT((GetLastError() & 0x0000FFFF) | 0x80070000);

		// CLSID\{...}\...
		HKEY hsubkey;
		LPCTSTR params[] = {
			_T(""),desc,NULL,
			_T("InprocServer32"),szModule,_T("ThreadingModel"),_T("Apartment"),NULL,
			_T("ProgID"),progID,NULL,
			_T("TypeLib"),buf,NULL,
			_T("Version"),version,NULL,
			NULL
		};

		writeGUID(typelib, buf);
		for (int i = 0; params[i]; i++)
		{
			LPCTSTR p1, p2;
			_tcscpy(op, p1 = params[i]);
			hr = RegCreateKeyEx(hkey, subkey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hsubkey, NULL);
			if (hr != ERROR_SUCCESS)
				break;
			p1 = NULL;
			do {
				if (!(p2 = params[i + 1]))
					break;
				hr = RegSetValueEx(hsubkey, p1, 0, REG_SZ, (const BYTE *)p2, DWORD((_tcslen(p2) + 1) * sizeof(TCHAR)));
			} while (hr == ERROR_SUCCESS && (p1 = params[i += 2]));
			RegCloseKey(hsubkey);
			if (hr != ERROR_SUCCESS)
				break;
		}

		// {ProgID}\CLSID
		if (hr == ERROR_SUCCESS)
		{
			op = subkey + sntprintf(subkey, MAX_PATH, _T("%s\\%s"), progID, _T("CLSID")) - 6;
			hr = RegCreateKeyEx(hkey, subkey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hsubkey, NULL);
			if (hr == ERROR_SUCCESS)
			{
				hr = RegSetValueEx(hsubkey, NULL, 0, REG_SZ, (const BYTE *)buf, DWORD((writeGUID(clsid, buf) + 1) * sizeof(TCHAR)));
				RegCloseKey(hsubkey), *op = 0;
				if (RegCreateKeyEx(hkey, subkey, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hsubkey, NULL) == ERROR_SUCCESS)
				{
					RegSetValueEx(hsubkey, NULL, 0, REG_SZ, (const BYTE *)desc, DWORD((_tcslen(desc) + 1) * sizeof(TCHAR)));
					RegCloseKey(hsubkey);
				}
			}
		}

		if (hr != ERROR_SUCCESS)
			DllInstall(FALSE, pszCmdLine);
	}
	else
	{
		hr = RegDeleteTree(hkey, _T("AutoHotkey2.Script"));
		auto hr2 = RegDeleteTree(hkey, subkey);
		hr = hr > ERROR_FILE_NOT_FOUND ? hr : hr2 > ERROR_FILE_NOT_FOUND ? hr2 : 0;
	}

	if (hkey != HKEY_CLASSES_ROOT)
		RegCloseKey(hkey);

	return HRESULT(hr ? (hr & 0x0000FFFF) | 0x80070000 : 0);
}


STDAPI DllRegisterServer()
{
	return DllInstall(TRUE, nullptr);
}


STDAPI DllUnregisterServer()
{
	return DllInstall(FALSE, nullptr);
}