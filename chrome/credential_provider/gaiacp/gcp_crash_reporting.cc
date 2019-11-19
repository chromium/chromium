// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_crash_reporting.h"

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporter_client.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporting_utils.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "components/crash/content/app/crashpad.h"
#include "content/public/common/content_switches.h"

namespace {

class GcpDllCrashReporterClient
    : public credential_provider::GcpCrashReporterClient {
 public:
  GcpDllCrashReporterClient() = default;
  ~GcpDllCrashReporterClient() override = default;

 protected:
  base::FilePath GetPathForFileVersionInfo(
      const base::string16& exe_path) override;
};

// When the DLL is loaded through rundll32. |exe_path| will point to
// rundll32.exe and the default version query on |exe_path| will query the
// version for rundll32.exe instead of querying the version of the DLL that
// contains the credential provider.
base::FilePath GcpDllCrashReporterClient::GetPathForFileVersionInfo(
    const base::string16& exe_path) {
  base::FilePath path_to_current_dll;
  HRESULT hr = credential_provider::GetPathToDllFromHandle(
      CURRENT_MODULE(), &path_to_current_dll);
  if (FAILED(hr))
    return GcpCrashReporterClient::GetPathForFileVersionInfo(exe_path);

  return path_to_current_dll;
}

}  // namespace

namespace credential_provider {

void ConfigureGcpCrashReporting(const base::CommandLine& command_line) {
  // This is inspired by work done in various parts of Chrome startup to connect
  // to the crash service. Since the installer does not split its work between
  // a stub .exe and a main .dll, crash reporting can be configured in one place
  // right here.
  // Create the crash client and install it (a la MainDllLoader::Launch).
  GcpDllCrashReporterClient* crash_client = new GcpDllCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(crash_client);

  InitializeGcpwCrashReporting(crash_client);

  base::CommandLine dll_main_cmd_line(base::CommandLine::NO_PROGRAM);

  HRESULT hr = GetCommandLineForEntrypoint(
      CURRENT_MODULE(), kRunAsCrashpadHandlerEntryPoint, &dll_main_cmd_line);
  if (hr == S_FALSE) {
    LOGFN(INFO) << "Failed to get command line to run crashpad handler";
    return;
  }
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetCommandLineForEntryPoint hr=" << putHR(hr);
    return;
  }

  // Check if this is the initial client for the crashpad handler or a sub
  // process started by the GCPW dll (e.g. the SaveAccountInfo process). Only
  // the initial client should start the crashpad handler process. Once this
  // process is started it will communicate via a named pipe (name is saved in a
  // process level environment variable) with all processes that wish to report
  // a crash. Subsequent child processes that are started should specify a
  // "process-type" flag so that they can re-use the shared named pipe without
  // having to start another crash handler process.
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  crash_reporter::InitializeCrashpadWithDllEmbeddedHandler(
      process_type.empty(), "GCPW DLL", "", dll_main_cmd_line.GetProgram(),
      {base::UTF16ToUTF8(dll_main_cmd_line.GetArgs()[0])});

  SetCommonCrashKeys(command_line);
}

}  // namespace credential_provider
