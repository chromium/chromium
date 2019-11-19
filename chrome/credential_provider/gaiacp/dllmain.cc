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

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_module.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "components/crash/content/app/crash_switches.h"
#include "components/crash/content/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"

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

// Used to determine whether the DLL can be unloaded by OLE.
STDAPI DllCanUnloadNow(void) {
  HRESULT hr = _AtlModule.DllCanUnloadNow();
  LOGFN(INFO) << "hr=" << putHR(hr);
  return hr;
}

// Returns a class factory to create an object of the requested type.
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  // Check to see if the credential provider has crashed too much recently.
  // If it has then do not allow it to create any credential providers.
  if (!credential_provider::VerifyStartupSentinel()) {
    LOGFN(ERROR) << "Disabled due to previous unsuccessful starts";
    return E_NOTIMPL;
  }

  HRESULT hr = _AtlModule.DllGetClassObject(rclsid, riid, ppv);

  // Start refreshing token handle validity as soon as possible so that when
  // their validity is requested later on by the credential providers they may
  // already be available and no wait is needed.
  if (SUCCEEDED(hr))
    _AtlModule.RefreshTokenHandleValidity();

  return hr;
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
      sts = key.WriteValue(
          L"name",
          credential_provider::GetStringResource(IDS_PROJNAME_BASE).c_str());
      if (sts != ERROR_SUCCESS) {
        hr = HRESULT_FROM_WIN32(sts);
        LOGFN(ERROR) << "key.WriteValue(name) hr=" << putHR(hr);
      }
    }
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return hr;
}

// DllUnregisterServer - Removes entries from the system registry.
STDAPI DllUnregisterServer(void) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
// CGaiaCredential::ForkSaveAccountInfoStub() for details.
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

  // First, read the buffer size.
  DWORD buffer_size = 0;
  DWORD bytes_read = 0;
  if (!::ReadFile(hStdin, &buffer_size, sizeof(buffer_size), &bytes_read,
                  nullptr)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ReadFile for buffer size failed. hr=" << putHR(hr);
    return;
  }

  // For security, we check for a max of 1 MB buffer size.
  const DWORD kMaxBufferSizeAllowed = 1024 * 1024;  // 1MB
  if (!buffer_size || buffer_size > kMaxBufferSizeAllowed) {
    LOGFN(ERROR) << "Invalid buffer size.";
    return;
  }

  // Second, read the buffer.
  std::vector<char> buffer(buffer_size, 0);
  if (!::ReadFile(hStdin, buffer.data(), buffer.size(), &bytes_read, nullptr)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ReadFile hr=" << putHR(hr);
    return;
  }
  // Don't log |buffer| since it contains sensitive info like password.

  HRESULT hr = S_OK;
  base::Optional<base::Value> properties =
      base::JSONReader::Read(buffer.data(), base::JSON_ALLOW_TRAILING_COMMAS);

  credential_provider::SecurelyClearBuffer(buffer.data(), buffer.size());

  if (!properties || !properties->is_dict()) {
    LOGFN(ERROR) << "base::JSONReader::Read failed length=" << buffer.size();
    return;
  }

  hr = credential_provider::CGaiaCredentialBase::SaveAccountInfo(*properties);
  if (FAILED(hr))
    LOGFN(ERROR) << "SaveAccountInfoW hr=" << putHR(hr);

  // Make sure COM is initialized in this thread. This thread must be
  // initialized as an MTA or the call to enroll with MDM causes a crash in COM.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedCOMInitializer failed hr=" << putHR(hr);
  } else {
    // Try to enroll the machine to MDM here. MDM requires a user to be signed
    // on to an interactive session to succeed and when we call this function
    // the user should have been successfully signed on at that point and able
    // to finish the enrollment.
    HRESULT hr = credential_provider::EnrollToGoogleMdmIfNeeded(*properties);
    if (FAILED(hr))
      LOGFN(ERROR) << "EnrollToGoogleMdmIfNeeded hr=" << putHR(hr);
  }

  credential_provider::SecurelyClearDictionaryValue(&properties);

  LOGFN(INFO) << "Done";
}

void CALLBACK RunAsCrashpadHandlerW(HWND /*hwnd*/,
                                    HINSTANCE /*hinst*/,
                                    wchar_t* /*pszCmdLine*/,
                                    int /*show*/) {
  base::CommandLine::Init(0, nullptr);

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  DCHECK_EQ(cmd_line->GetSwitchValueASCII(switches::kProcessType),
            crash_reporter::switches::kCrashpadHandler);

  base::string16 entrypoint_arg;
  credential_provider::GetEntryPointArgumentForRunDll(
      CURRENT_MODULE(), credential_provider::kRunAsCrashpadHandlerEntryPoint,
      &entrypoint_arg);

  // Get all the arguments from the original command line except for the
  // entrypoint argument otherwise the crashpad handler will fail to start
  // thinking that it is an invalid argument.
  base::CommandLine::StringVector argv_without_entry_point;
  argv_without_entry_point.reserve(cmd_line->argv().size());
  for (const auto& argv : cmd_line->argv()) {
    if (argv == entrypoint_arg)
      continue;

    argv_without_entry_point.push_back(argv);
  }

  base::CommandLine cmd_line_without_entry_point(argv_without_entry_point);

  crash_reporter::RunAsCrashpadHandler(cmd_line_without_entry_point,
                                       base::FilePath(), switches::kProcessType,
                                       "user-data-dir");
}

void CALLBACK
SetFakesForTesting(const credential_provider::FakesForTesting* fakes) {
  DCHECK(fakes);
  credential_provider::ScopedLsaPolicy::SetCreatorForTesting(
      fakes->scoped_lsa_policy_creator);
  credential_provider::OSUserManager::SetInstanceForTesting(
      fakes->os_user_manager_for_testing);
  credential_provider::OSProcessManager::SetInstanceForTesting(
      fakes->os_process_manager_for_testing);

  _AtlModule.set_is_testing(true);
}
