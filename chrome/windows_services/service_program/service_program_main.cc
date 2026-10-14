// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/service_program_main.h"

#include <windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome/windows_services/service_program/crashpad_handler.h"
#include "chrome/windows_services/service_program/logging_support.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "chrome/windows_services/service_program/service.h"
#include "chrome/windows_services/service_program/service_delegate.h"

namespace {

// Refuses to follow NTFS junctions that were not created by an elevated actor,
// failing such traversals with ERROR_UNTRUSTED_MOUNT_POINT. This blocks the
// attack in which an unprivileged caller plants a junction to redirect a
// privileged service's file operation to a location of the caller's choosing.
// This governs every file operation the process performs. The mitigation cannot
// be disabled once applied; it remains in effect for the lifetime of the
// process.
//
// Requires Windows 11 22H2 or a serviced Windows 10; silently skipped on
// earlier versions.
void EnableRedirectionGuard() {
  PROCESS_MITIGATION_REDIRECTION_TRUST_POLICY policy = {};
  policy.EnforceRedirectionTrust = 1;
  if (::SetProcessMitigationPolicy(ProcessRedirectionTrustPolicy, &policy,
                                   sizeof(policy))) {
    return;
  }

  // Versions of Windows that predate Redirection Guard reject the policy.
  // That is expected, so do not log it.
  const DWORD error = ::GetLastError();
  PLOG_IF(ERROR,
          error != ERROR_INVALID_PARAMETER && error != ERROR_NOT_SUPPORTED)
      << "Failed to enable Redirection Guard";
}

}  // namespace

int ServiceProgramMain(ServiceDelegate& delegate) {
  // Initialize the CommandLine singleton from the environment.
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* const cmd_line = base::CommandLine::ForCurrentProcess();

  InitializeLogging(*cmd_line);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  install_static::InitializeProductDetailsForPrimaryModule();

  // Enable logging to the Windows Event Log.
  logging::SetEventSource(
      base::WideToUTF8(
          install_static::InstallDetails::Get().install_full_name()),
      delegate.GetLogEventCategory(), delegate.GetLogEventMessageId());

  if (auto optional_result = RunAsCrashpadHandlerIfRequired(*cmd_line);
      optional_result.has_value()) {
    return *optional_result;
  }

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*cmd_line);

  EnableRedirectionGuard();

  // Initialize COM for the current thread.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    return -1;
  }

  // Create the global WRL::Module instance.
  CreateWrlModule();

  // Register an empty FeatureList so that queries on it do not fail.
  base::FeatureList::SetInstance(std::make_unique<base::FeatureList>());

  // Run the COM service.
  auto service = base::MakeRefCounted<Service>(delegate);

  return service->InitWithCommandLine(cmd_line) ? service->Start() : -1;
}
