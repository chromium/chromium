// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This app is written as a windowsless win32 app instead of a console app so
// that the app can be made entireless silent, as required by omaha.

#include <Windows.h>
#include <shlobj.h> // Needed for IsUserAnAdmin()

#include <stdlib.h>
#include <string>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // make some CString ctors explicit

#include <atlbase.h>
#include <atlstr.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/setup/setup_lib.h"

using credential_provider::putHR;

namespace {

bool IsPerUserInstallFromGoogleUpdate() {
  wchar_t value[2];
  DWORD length = ::GetEnvironmentVariable(L"GoogleUpdateIsMachine", value,
                                          base::size(value));

  return length == 1 && value[0] == L'0';
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE /*hPrevInstance*/,
                      wchar_t* lpCmdLine,
                      int /*nCmdShow*/) {
  HRESULT hr = S_OK;

  // Initialize base.  Command line will be set from GetCommandLineW().
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, nullptr);

  // Initialize logging.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_NONE;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // Enable process id.
                       true,    // Enable thread id.
                       true,    // Enable timestamp.
                       false);  // Enable tickcount.
  logging::SetEventSource("GCP", GCP_CATEGORY, MSG_LOG_MESSAGE);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());

  wchar_t module[MAX_PATH];
  DWORD module_length = ::GetModuleFileName(hInstance, module,
                                            base::size(module));
  if (module_length == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetModuleFileName hr=" << putHR(hr);
    return -1;
  } else if (module_length == base::size(module)) {
    LOGFN(ERROR) << "GetModuleFileName: module name too long";
    return -1;
  }

  base::FilePath gcp_setup_exe_path(base::StringPiece16(module, module_length));

  wchar_t time_string[64];
  if (::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, nullptr, nullptr,
                        time_string, base::size(time_string)) == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetTimeFormatEx(start) hr=" << putHR(hr);
    wcscpy_s(time_string, base::size(time_string), L"Unknown");
  }

  LOGFN(INFO) << "Start: " << time_string;
  LOGFN(INFO) << "Module: " << gcp_setup_exe_path;
  LOGFN(INFO) << "Args: " << lpCmdLine;
  LOGFN(INFO) << "Version: " << TEXT(CHROME_VERSION_STRING);
  LOGFN(INFO) << "Windows: "
              << base::win::OSInfo::GetInstance()->Kernel32BaseVersion();

  // If running from omaha, make sure machine install is used.
  if (IsPerUserInstallFromGoogleUpdate()) {
    LOGFN(ERROR) << "Only machine installs supported with Google Update";
    return -1;
  }

  if (!::IsUserAnAdmin()) {
    LOGFN(ERROR) << "Setup must be run with administrative privilege.";
    return -1;
  }

  hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Could not initialize COM.";
    return -1;
  }

  // Parse command line.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool is_uninstall = cmdline->HasSwitch(
      credential_provider::switches::kUninstall);
  base::FilePath path = cmdline->GetSwitchValuePath(
      credential_provider::switches::kInstallPath);
  std::string parent_handle_str =
      cmdline->GetSwitchValueASCII(
          credential_provider::switches::kParentHandle);

  if (is_uninstall) {
    // If this is a user invoked uninstall, copy the exe to the temp directory
    // and rerun it from there.  Append a new arg so that setup knows it is not
    // user invoked and where to uninstall from.
    if (path.empty()) {
      hr = credential_provider::RelaunchUninstaller(gcp_setup_exe_path);
    } else {
      // Wait for parent process to exit.  Proceed in any case.
      if (!parent_handle_str.empty()) {
        uint32_t parent_handle_value;
        if (base::StringToUint(parent_handle_str, &parent_handle_value)) {
          base::win::ScopedHandle parent_handle(
              base::win::Uint32ToHandle(parent_handle_value));
          DWORD ret = ::WaitForSingleObject(parent_handle.Get(), 5000);
          LOGFN(INFO) << "Waited for parent(" << parent_handle.Get()
                      << "): ret=" << ret;
        }
      }

      hr = credential_provider::DoUninstall(gcp_setup_exe_path, path, nullptr);

      // Schedule the installer to be deleted on the next reboot.
      if (!base::DeleteFileAfterReboot(gcp_setup_exe_path)) {
        HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "DeleteFileAfterReboot hr=" << putHR(hr);
      }
    }
  } else {
    hr = credential_provider::DoInstall(gcp_setup_exe_path,
                                        TEXT(CHROME_VERSION_STRING), nullptr);
  }

  // Log success or failure only if uninstall was not launched as a separate
  // process.
  if (!(is_uninstall && path.empty())) {
    if (::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, nullptr, nullptr,
                          time_string, base::size(time_string)) == 0) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "GetTimeFormatEx(end) hr=" << putHR(hr);
      wcscpy_s(time_string, base::size(time_string), L"Unknown");
    }

    LOGFN(INFO) << (SUCCEEDED(hr) ? "Setup completed successfully"
                                  : "Setup failed")
                << ". " << time_string;
  }

  ::CoUninitialize();
  return 0;
}
