// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_module.h"

#include <process.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/extension/extension_utils.h"
#include "chrome/credential_provider/extension/os_service_manager.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_filter.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporting.h"
#include "chrome/credential_provider/gaiacp/grit/gaia_static_resources.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_switches.h"

namespace credential_provider {

namespace {

void InvalidParameterHandler(const wchar_t* expression,
                             const wchar_t* function,
                             const wchar_t* file,
                             unsigned int line,
                             uintptr_t pReserved) {
  LOGFN(ERROR) << "func=" << (function ? function : L"-")
               << " expression=" << (expression ? expression : L"-")
               << " file=" << (file ? file : L"-") << " line=" << line;
}

unsigned __stdcall CheckGCPWExtensionStatus(void* param) {
  LOGFN(VERBOSE);
  if (!credential_provider::extension::IsGCPWExtensionRunning()) {
    credential_provider::extension::OSServiceManager* service_manager =
        credential_provider::extension::OSServiceManager::Get();

    DWORD error_code = service_manager->StartGCPWService();
    if (error_code != ERROR_SUCCESS) {
      LOGFN(WARNING) << "Unable to start GCPW extension win32=" << error_code;
    }
  }
  return 0;
}

}  // namespace

CGaiaCredentialProviderModule::CGaiaCredentialProviderModule()
    : ATL::CAtlDllModuleT<CGaiaCredentialProviderModule>(),
      exit_manager_(nullptr),
      gcpw_extension_check_performed_(0),
      crashpad_initialized_(0) {}

CGaiaCredentialProviderModule::~CGaiaCredentialProviderModule() {}

// static
HRESULT WINAPI
CGaiaCredentialProviderModule::UpdateRegistryAppId(BOOL do_register) throw() {
  base::FilePath eventlog_path;
  HRESULT hr = CGaiaCredentialBase::GetInstallDirectory(&eventlog_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "CGaiaCredentialBase::GetInstallDirectory hr=" << putHR(hr);
    return hr;
  }
  eventlog_path =
      eventlog_path.Append(FILE_PATH_LITERAL("gcp_eventlog_provider.dll"));

  auto provider_guid_string =
      base::win::WStringFromGUID(CLSID_GaiaCredentialProvider);

  auto filter_guid_string =
      base::win::WStringFromGUID(CLSID_CGaiaCredentialProviderFilter);

  ATL::_ATL_REGMAP_ENTRY regmap[] = {
      {L"CP_CLASS_GUID", provider_guid_string.c_str()},
      {L"CP_FILTER_CLASS_GUID", filter_guid_string.c_str()},
      {L"VERSION", TEXT(CHROME_VERSION_STRING)},
      {L"EVENTLOG_PATH", eventlog_path.value().c_str()},
      {nullptr, nullptr},
  };

  return ATL::_pAtlModule->UpdateRegistryFromResource(
      IDR_GAIACREDENTIALPROVIDER, do_register, regmap);
}

void CGaiaCredentialProviderModule::RefreshTokenHandleValidity() {
  if (!token_handle_validity_refreshed_) {
    credential_provider::AssociatedUserValidator::Get()
        ->StartRefreshingTokenHandleValidity();
    token_handle_validity_refreshed_ = true;
  }
}

void CGaiaCredentialProviderModule::CheckGCPWExtension() {
  LOGFN(VERBOSE);
  if (extension::IsGCPWExtensionEnabled() &&
      ::InterlockedCompareExchange(&gcpw_extension_check_performed_, 1, 0) ==
          0) {
    gcpw_extension_checker_thread_handle_ =
        reinterpret_cast<HANDLE>(_beginthreadex(
            nullptr, 0, CheckGCPWExtensionStatus, nullptr, 0, nullptr));
  }
}

void CGaiaCredentialProviderModule::InitializeCrashReporting() {
  // Perform a thread unsafe check to see whether crash reporting is
  // initialized. Thread safe check is performed right before initializing crash
  // reporting.
  if (GetGlobalFlagOrDefault(kRegInitializeCrashReporting, 1) &&
      !crashpad_initialized_) {
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

    if (!base::EndsWith(cmd_line->GetProgram().value(), L"gcp_unittests.exe",
                        base::CompareCase::INSENSITIVE_ASCII) &&
        cmd_line->GetSwitchValueASCII(switches::kProcessType) !=
            crash_reporter::switches::kCrashpadHandler &&
        ::InterlockedCompareExchange(&crashpad_initialized_, 1, 0) == 0) {
      ConfigureGcpCrashReporting(*cmd_line);
      LOGFN(VERBOSE) << "Crash reporting was initialized.";
    }
  }
}

void CGaiaCredentialProviderModule::LogProcessDetails() {
  wchar_t process_name[MAX_PATH] = {0};
  GetModuleFileName(nullptr, process_name, MAX_PATH);

  LOGFN(INFO) << "GCPW Initialized in " << process_name
              << " GCPW Version: " << (CHROME_VERSION_STRING)
              << " Windows Build: " << base::win::OSInfo::Kernel32BaseVersion()
              << " Version:" << GetWindowsVersion();
}

BOOL CGaiaCredentialProviderModule::DllMain(HINSTANCE /*hinstance*/,
                                            DWORD reason,
                                            LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH: {
      exit_manager_ = std::make_unique<base::AtExitManager>();

      _set_invalid_parameter_handler(InvalidParameterHandler);

      // Initialize base.  Command line will be set from GetCommandLineW().
      base::CommandLine::Init(0, nullptr);

      // Initialize logging.
      logging::LoggingSettings settings;
      settings.logging_dest = logging::LOG_NONE;

      std::wstring log_file_path =
          GetGlobalFlagOrDefault(kRegLogFilePath, std::wstring{});
      if (not log_file_path.empty()) {
        settings.logging_dest = logging::LOG_TO_FILE;
        bool append_log = GetGlobalFlagOrDefault(kRegLogFileAppend, 0);
        settings.delete_old = append_log ? logging::APPEND_TO_OLD_LOG_FILE
                                         : logging::DELETE_OLD_LOG_FILE;
        settings.log_file_path = log_file_path;
      }

      logging::InitLogging(settings);
      logging::SetLogItems(true,    // Enable process id.
                           true,    // Enable thread id.
                           true,    // Enable timestamp.
                           false);  // Enable tickcount.
      logging::SetEventSource("GCPW", GCPW_CATEGORY, MSG_LOG_MESSAGE);
      if (GetGlobalFlagOrDefault(kRegEnableVerboseLogging, 0))
        logging::SetMinLogLevel(logging::LOGGING_VERBOSE);
      break;
    }
    case DLL_PROCESS_DETACH:
      LOGFN(VERBOSE) << "DllMain(DLL_PROCESS_DETACH)";

      // When this DLL is loaded for testing, don't reset the command line
      // since it causes tests to crash.
      if (!is_testing_)
        base::CommandLine::Reset();

      _set_invalid_parameter_handler(nullptr);
      exit_manager_.reset();

      crash_reporter::DestroyCrashpadClient();
      break;

    default:
      break;
  }

  return ATL::CAtlDllModuleT<CGaiaCredentialProviderModule>::DllMain(reason,
                                                                     reserved);
}

}  // namespace credential_provider
