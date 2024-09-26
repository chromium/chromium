// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/chrome_elf_main.h"

#include <windows.h>

#include <assert.h>

#include "chrome/chrome_elf/chrome_elf_security.h"
#include "chrome/chrome_elf/crash/crash_helper.h"
#include "chrome/chrome_elf/third_party_dlls/beacon.h"
#include "chrome/chrome_elf/third_party_dlls/main.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome/install_static/user_data_dir.h"

// This function is exported from the DLL so that it can be called by WinMain
// after startup has completed in the browser process. For non-browser processes
// it will be called inside the DLL loader lock so it should do as little as
// possible to prevent deadlocks.
void SignalInitializeCrashReporting() {
  if (!elf_crash::InitializeCrashReporting()) {
#ifdef _DEBUG
    assert(false);
#endif  // _DEBUG
  }
}

void SignalChromeElf() {
  third_party_dlls::ResetBeacon();
}

bool GetUserDataDirectoryThunk(wchar_t* user_data_dir,
                               size_t user_data_dir_length,
                               wchar_t* invalid_user_data_dir,
                               size_t invalid_user_data_dir_length) {
  std::wstring user_data_dir_str, invalid_user_data_dir_str;
  bool ret = install_static::GetUserDataDirectory(&user_data_dir_str,
                                                  &invalid_user_data_dir_str);
  assert(ret);
  install_static::IgnoreUnused(ret);
  wcsncpy_s(user_data_dir, user_data_dir_length, user_data_dir_str.c_str(),
            _TRUNCATE);
  wcsncpy_s(invalid_user_data_dir, invalid_user_data_dir_length,
            invalid_user_data_dir_str.c_str(), _TRUNCATE);

  return true;
}

bool IsTemporaryUserDataDirectoryCreatedForHeadless() {
  return install_static::IsTemporaryUserDataDirectoryCreatedForHeadless();
}

// DllMain
// -------
// Warning: The OS loader lock is held during DllMain.  Be careful.
// https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971.aspx
//
// - Note: Do not use install_static::GetUserDataDir from inside DllMain.
//         This can result in path expansion that triggers secondary DLL loads,
//         that will blow up with the loader lock held.
//         https://bugs.chromium.org/p/chromium/issues/detail?id=748949#c18
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    install_static::InitializeProductDetailsForPrimaryModule();
    install_static::InitializeProcessType();

    if (install_static::IsBrowserProcess()) {
      __try {
        // Disable third party extension points.
        elf_security::EarlyBrowserSecurity();

        // Initialize the blocking of third-party DLLs if the initialization of
        // the safety beacon succeeds.
        if (third_party_dlls::LeaveSetupBeacon())
          third_party_dlls::Init();
      } __except (elf_crash::GenerateCrashDump(GetExceptionInformation())) {
      }
    } else if (!install_static::IsCrashpadHandlerProcess()) {
      SignalInitializeCrashReporting();
      // CRT on initialization installs an exception filter which calls
      // TerminateProcess. We need to hook CRT's attempt to set an exception.
      elf_crash::DisableSetUnhandledExceptionFilter();
    }

  } else if (reason == DLL_PROCESS_DETACH) {
    elf_crash::ShutdownCrashReporting();
  }
  return TRUE;
}

void DumpProcessWithoutCrash() {
  elf_crash::DumpWithoutCrashing();
}

void SetMetricsClientId(const char* client_id) {
  elf_crash::SetMetricsClientIdImpl(client_id);
}

bool IsBrowserProcess() {
  return install_static::IsBrowserProcess();
}

bool IsExtensionPointDisableSet() {
  return elf_security::IsExtensionPointDisableSet();
}
