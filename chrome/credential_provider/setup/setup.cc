// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This app is written as a windowsless win32 app instead of a console app so
// that the app can be made entireless silent, as required by omaha.

#include <Windows.h>
#include <shlobj.h>  // Needed for IsUserAnAdmin()
#include <stdlib.h>

#include <algorithm>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/setup/gcp_installer_crash_reporting.h"
#include "chrome/credential_provider/setup/setup_lib.h"
#include "chrome/credential_provider/setup/setup_utils.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"

using credential_provider::GetGlobalFlagOrDefault;
using credential_provider::kRegEnableVerboseLogging;
using credential_provider::MakeGcpwDefaultCP;
using credential_provider::putHR;

namespace {

bool IsPerUserInstallFromGoogleUpdate() {
  wchar_t value[2];
  DWORD length = ::GetEnvironmentVariable(L"GoogleUpdateIsMachine", value,
                                          std::size(value));

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

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  std::string process_type =
      cmdline->GetSwitchValueASCII(switches::kProcessType);

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(*cmdline, base::FilePath(),
                                                switches::kProcessType, "");
  }

  // Initialize logging.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_NONE;

  // See if the log file path was specified on the command line.
  base::FilePath log_file_path = cmdline->GetSwitchValuePath("log-file");
  if (!log_file_path.empty()) {
    settings.logging_dest = logging::LOG_TO_FILE;
    settings.log_file_path = log_file_path.value().c_str();
  }

  logging::InitLogging(settings);
  logging::SetLogItems(true,    // Enable process id.
                       true,    // Enable thread id.
                       true,    // Enable timestamp.
                       false);  // Enable tickcount.

  logging::SetEventSource("GCPW", GCPW_CATEGORY, MSG_LOG_MESSAGE);

  if (GetGlobalFlagOrDefault(kRegEnableVerboseLogging, 1))
    logging::SetMinLogLevel(logging::LOGGING_VERBOSE);

  // Set GCPW as the default credential provider for the end user.
  MakeGcpwDefaultCP();

  if (cmdline->HasSwitch(switches::kLoggingLevel)) {
    std::string log_level =
        cmdline->GetSwitchValueASCII(switches::kLoggingLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) && level >= 0 &&
        level < logging::LOGGING_NUM_SEVERITIES) {
      logging::SetMinLogLevel(level);
    } else {
      LOGFN(WARNING) << "Bad log level: " << log_level;
    }
  }

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());

  if (!::IsUserAnAdmin()) {
    LOGFN(ERROR) << "Setup must be run with administrative privilege.";
    return -1;
  }

  credential_provider::ConfigureGcpInstallerCrashReporting(*cmdline);

  // If the program is being run to either enable or disable stats, do that
  // and exit.
  if (cmdline->HasSwitch(credential_provider::switches::kEnableStats) ||
      cmdline->HasSwitch(credential_provider::switches::kDisableStats)) {
    return credential_provider::EnableStatsCollection(*cmdline);
  }

  base::FilePath gcp_setup_exe_path;
  hr = credential_provider::GetPathToDllFromHandle(hInstance,
                                                   &gcp_setup_exe_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetPathToDllFromHandle hr=" << putHR(hr);
    return -1;
  }

  wchar_t time_string[64];
  if (::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, nullptr, nullptr,
                        time_string, std::size(time_string)) == 0) {
    HRESULT last_error_hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetTimeFormatEx(start) hr=" << putHR(last_error_hr);
    wcscpy_s(time_string, std::size(time_string), L"Unknown");
  }

  LOGFN(INFO) << "Start: " << time_string;
  LOGFN(INFO) << "Module: " << gcp_setup_exe_path;
  LOGFN(INFO) << "Args: " << lpCmdLine;
  LOGFN(INFO) << "Version: " << TEXT(CHROME_VERSION_STRING);

  LOGFN(INFO) << "Windows: " << base::win::OSInfo::Kernel32BaseVersion()
              << " Version:" << credential_provider::GetWindowsVersion();

  // If running from omaha, make sure machine install is used.
  if (IsPerUserInstallFromGoogleUpdate()) {
    LOGFN(ERROR) << "Only machine installs supported with Google Update";
    return -1;
  }

  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);

  // Parse command line.
  bool is_uninstall =
      cmdline->HasSwitch(credential_provider::switches::kUninstall);
  base::FilePath path =
      cmdline->GetSwitchValuePath(credential_provider::switches::kInstallPath);
  std::string parent_handle_str = cmdline->GetSwitchValueASCII(
      credential_provider::switches::kParentHandle);

  credential_provider::StandaloneInstallerConfigurator::Get()
      ->ConfigureInstallationType(*cmdline);

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
          LOGFN(VERBOSE) << "Waited for parent(" << parent_handle.Get()
                         << "): ret=" << ret;
        }
      }

      hr = credential_provider::DoUninstall(gcp_setup_exe_path, path, nullptr);

      // Schedule the installer to be deleted on the next reboot.
      if (!base::DeleteFileAfterReboot(gcp_setup_exe_path)) {
        HRESULT last_error_hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "DeleteFileAfterReboot hr=" << putHR(last_error_hr);
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
                          time_string, std::size(time_string)) == 0) {
      HRESULT last_error_hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "GetTimeFormatEx(end) hr=" << putHR(last_error_hr);
      wcscpy_s(time_string, std::size(time_string), L"Unknown");
    }

    LOGFN(INFO) << (SUCCEEDED(hr) ? "Setup completed successfully"
                                  : "Setup failed")
                << ". " << time_string;
  }

  return 0;
}
