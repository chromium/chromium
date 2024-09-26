// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/service_program_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "chrome/windows_services/service_program/service.h"
#include "chrome/windows_services/service_program/service_delegate.h"

int ServiceProgramMain(ServiceDelegate& delegate) {
  // Initialize the CommandLine singleton from the environment.
  base::CommandLine::Init(0, nullptr);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  install_static::InitializeProductDetailsForPrimaryModule();

  // Enable logging to the Windows Event Log.
  logging::SetEventSource(
      base::WideToUTF8(
          install_static::InstallDetails::Get().install_full_name()),
      delegate.GetLogEventCategory(), delegate.GetLogEventMessageId());

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());

  // Initialize COM for the current thread.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    return -1;
  }

  // Create the global WRL::Module instance.
  CreateWrlModule();

  // Run the COM service.
  Service service(delegate);

  return service.InitWithCommandLine(base::CommandLine::ForCurrentProcess())
             ? service.Start()
             : -1;
}
