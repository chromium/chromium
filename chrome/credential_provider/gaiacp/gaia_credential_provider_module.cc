// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/dllmain.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_filter.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporting.h"
#include "chrome/credential_provider/gaiacp/grit/gaia_static_resources.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "components/crash/content/app/crash_switches.h"
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

}  // namespace

CGaiaCredentialProviderModule::CGaiaCredentialProviderModule()
    : ATL::CAtlDllModuleT<CGaiaCredentialProviderModule>(),
      exit_manager_(nullptr) {}

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
      base::win::String16FromGUID(CLSID_GaiaCredentialProvider);

  auto filter_guid_string =
      base::win::String16FromGUID(CLSID_CGaiaCredentialProviderFilter);

  ATL::_ATL_REGMAP_ENTRY regmap[] = {
      {L"CP_CLASS_GUID", base::as_wcstr(provider_guid_string.c_str())},
      {L"CP_FILTER_CLASS_GUID", base::as_wcstr(filter_guid_string.c_str())},
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
      logging::InitLogging(settings);
      logging::SetLogItems(true,    // Enable process id.
                           true,    // Enable thread id.
                           true,    // Enable timestamp.
                           false);  // Enable tickcount.
      logging::SetEventSource("GCPW", GCPW_CATEGORY, MSG_LOG_MESSAGE);

      base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

      // Don't start the crash handler if the DLL is being loaded in unit
      // tests. It is possible that the DLL will be loaded by other executables
      // including gcp_setup.exe, LogonUI.exe, rundll32.exe.
      if (!base::EndsWith(cmd_line->GetProgram().value(), L"gcp_unittests.exe",
                          base::CompareCase::INSENSITIVE_ASCII) &&
          cmd_line->GetSwitchValueASCII(switches::kProcessType) !=
              crash_reporter::switches::kCrashpadHandler) {
        credential_provider::ConfigureGcpCrashReporting(*cmd_line);
      }

      LOGFN(INFO) << "DllMain(DLL_PROCESS_ATTACH) Build: "
                  << base::win::OSInfo::GetInstance()->Kernel32BaseVersion()
                  << " Version:" << GetWindowsVersion();
      break;
    }
    case DLL_PROCESS_DETACH:
      LOGFN(INFO) << "DllMain(DLL_PROCESS_DETACH)";

      // When this DLL is loaded for testing, don't reset the command line
      // since it causes tests to crash.
      if (!is_testing_)
        base::CommandLine::Reset();

      _set_invalid_parameter_handler(nullptr);
      exit_manager_.reset();
      break;

    default:
      break;
  }

  return ATL::CAtlDllModuleT<CGaiaCredentialProviderModule>::DllMain(reason,
                                                                     reserved);
}

}  // namespace credential_provider
