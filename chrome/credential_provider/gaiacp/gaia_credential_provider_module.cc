// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/dllmain.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"

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

  wchar_t guid_in_wchar[64];
  StringFromGUID2(CLSID_GaiaCredentialProvider, guid_in_wchar, base::size(guid_in_wchar));

  ATL::_ATL_REGMAP_ENTRY regmap[] = {
      {L"CREDENTIAL_PROVIDER_CLASS_GUID", guid_in_wchar},
      {L"VERSION", TEXT(CHROME_VERSION_STRING)},
      {L"EVENTLOG_PATH", eventlog_path.value().c_str()},
      {nullptr, nullptr},
  };

  return ATL::_pAtlModule->UpdateRegistryFromResource(
      IDR_GAIACREDENTIALPROVIDER, do_register, regmap);
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
      logging::SetEventSource("GCP", GCP_CATEGORY, MSG_LOG_MESSAGE);
      LOGFN(INFO) << "DllMain(DLL_PROCESS_ATTACH)";
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
