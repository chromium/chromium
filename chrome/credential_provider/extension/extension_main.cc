// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "windows.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/win/process_startup_helper.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/extension/os_service_manager.h"
#include "chrome/credential_provider/extension/service.h"
#include "chrome/credential_provider/gaiacp/logging.h"

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE /*hPrevInstance*/,
                      wchar_t* lpCmdLine,
                      int /*nCmdShow*/) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(0, nullptr);
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

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

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());

  // Set the event logging source and category for GCPW Extension.
  logging::SetEventSource("GCPW", GCPW_EXTENSION_CATEGORY, MSG_LOG_MESSAGE);

  credential_provider::extension::Service::Get()->Run();

  return 0;
}
