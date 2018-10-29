// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/dllmain.h"

// It is important to include gaia_credential_base.h, reauth_credential.h and
// gaia_credential.h here, even though the classes are not used in this source
// file, in order to ensure that the COM objects will be properly linked into
// the dll.  This became important after moving all the COM object code into a
// static library for testing purposes.
//
// This article clearly describes the issue and the fix:
//
// https://blogs.msdn.microsoft.com/larryosterman/2004/09/27/when-i-moved-my-code-into-a-library-what-happened-to-my-atl-com-objects/

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_module.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

using credential_provider::putHR;

credential_provider::CGaiaCredentialProviderModule _AtlModule;

#define __TYPELIB_ID_SUFFIX_STRING(id) L"\\" _T(#id)
#define _TYPELIB_ID_SUFFIX_STRING(id) __TYPELIB_ID_SUFFIX_STRING(id)
#define TYPELIB_SUFFIX_STRING \
  _TYPELIB_ID_SUFFIX_STRING(TLB_GAIACREDENTIALPROVIDER)

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hinstance,
                               DWORD reason,
                               LPVOID reserved) {
  return _AtlModule.DllMain(hinstance, reason, reserved);
}

using namespace ATL;

// Used to determine whether the DLL can be unloaded by OLE.
STDAPI DllCanUnloadNow(void) {
  HRESULT hr = _AtlModule.DllCanUnloadNow();
  LOGFN(INFO) << "hr=" << putHR(hr);
  return hr;
}

// Returns a class factory to create an object of the requested type.
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry.
STDAPI DllRegisterServer(void) {
  HRESULT hr = credential_provider::CGaiaCredentialBase::OnDllRegisterServer();
  LOGFN(INFO) << "CGaiaCredential::OnDllRegisterServer hr=" << putHR(hr);

  if (SUCCEEDED(hr)) {
    // Registers object.  FALSE means don't register typelib.  The default
    // behaviour is assume the typelib has ID 1.  But in this case grit can't
    // be forced to use an ID of 1 when writing the rc file.
    hr = _AtlModule.DllRegisterServer(FALSE);
    LOGFN(INFO) << "_AtlModule.DllRegisterServer hr=" << putHR(hr);
  }

#if defined(GOOGLE_CHROME_BUILD)
  // Register with Google Update.
  if (SUCCEEDED(hr)) {
    base::win::RegKey key(HKEY_LOCAL_MACHINE,
                          credential_provider::kRegUpdaterClientsAppPath,
                          KEY_SET_VALUE | KEY_WOW64_32KEY);
    LONG sts = key.WriteValue(L"pv", TEXT(CHROME_VERSION_STRING));
    if (sts != ERROR_SUCCESS) {
      hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.WriteValue(pv) hr=" << putHR(hr);
    } else {
      sts = key.WriteValue(L"name",
          credential_provider::GetStringResource(IDS_PROJNAME).c_str());
      if (sts != ERROR_SUCCESS) {
        hr = HRESULT_FROM_WIN32(sts);
        LOGFN(ERROR) << "key.WriteValue(name) hr=" << putHR(hr);
      }
    }
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  return hr;
}

// DllUnregisterServer - Removes entries from the system registry.
STDAPI DllUnregisterServer(void) {
#if defined(GOOGLE_CHROME_BUILD)
  // Unregister with Google Update.
  base::win::RegKey key(HKEY_LOCAL_MACHINE, L"", DELETE | KEY_WOW64_32KEY);
  LONG sts = key.DeleteKey(credential_provider::kRegUpdaterClientsAppPath);

  bool all_succeeded = sts == ERROR_SUCCESS;
#else
  bool all_succeeded = true;
#endif

  HRESULT hr =
      credential_provider::CGaiaCredentialBase::OnDllUnregisterServer();
  LOGFN(INFO) << "CGaiaCredential::OnDllUnregisterServer hr=" << putHR(hr);
  all_succeeded &= SUCCEEDED(hr);

  hr = _AtlModule.DllUnregisterServer(FALSE);
  LOGFN(INFO) << "_AtlModule.DllUnregisterServer hr=" << putHR(hr);
  all_succeeded &= SUCCEEDED(hr);

  return all_succeeded ? S_OK : E_FAIL;
}

// This entry point is called via rundll32.  See
// CGaiaCredential::WaitForLoginUI() for details.
void CALLBACK SaveAccountInfoW(HWND /*hwnd*/,
                               HINSTANCE /*hinst*/,
                               wchar_t* /*pszCmdLine*/,
                               int /*show*/) {
  LOGFN(INFO);

  HANDLE hStdin = ::GetStdHandle(STD_INPUT_HANDLE);  // No need to close.
  if (hStdin == INVALID_HANDLE_VALUE) {
    LOGFN(INFO) << "No stdin";
    return;
  }

  char buffer[credential_provider::CGaiaCredentialBase::kAccountInfoBufferSize];
  DWORD buffer_len_bytes = static_cast<DWORD>(sizeof(buffer));  // In bytes.
  if (!::ReadFile(hStdin, buffer, buffer_len_bytes, &buffer_len_bytes,
                  nullptr)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ReadFile hr=" << putHR(hr);
    return;
  }
  buffer[buffer_len_bytes] = 0;
  // Don't log |buffer| since it contains sensitive info like password.

  HRESULT hr = S_OK;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> properties =
      base::JSONReader::Read(buffer, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!properties || !properties->GetAsDictionary(&dict)) {
    LOGFN(ERROR) << "base::JSONReader::Read failed length=" << buffer_len_bytes;
    hr = E_FAIL;
  }

  hr = credential_provider::CGaiaCredentialBase::SaveAccountInfo(*dict);
  if (FAILED(hr))
    LOGFN(ERROR) << "SaveAccountInfoW hr=" << putHR(hr);

  // If an MDM URL is configured in the registry, use it.

  wchar_t mdm_url[256];
  ULONG length = base::size(mdm_url);
  hr = credential_provider::GetGlobalFlag(L"mdm", mdm_url, &length);
  if (SUCCEEDED(hr)) {
    dict->SetString(credential_provider::kKeyMdmUrl, mdm_url);

    hr = credential_provider::EnrollToGoogleMdmIfNeeded(*dict);
    if (FAILED(hr))
      LOGFN(INFO) << "EnrollToGoogleMdmIfNeeded hr=" << putHR(hr);
  } else {
    LOGFN(INFO) << "Not enrolling to MDM";
  }

  LOGFN(INFO) << "Done";
}

void CALLBACK SetFakesForTesting(
    const credential_provider::FakesForTesting* fakes) {
  DCHECK(fakes);
  credential_provider::ScopedLsaPolicy::SetCreatorForTesting(
      fakes->scoped_lsa_policy_creator);
  credential_provider::OSUserManager::SetInstanceForTesting(
      fakes->os_manager_for_testing);

  _AtlModule.set_is_testing(true);
}
