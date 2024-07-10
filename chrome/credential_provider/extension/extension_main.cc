// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/win/process_startup_helper.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/eventlog/gcp_eventlog_messages.h"
#include "chrome/credential_provider/extension/app_inventory_manager.h"
#include "chrome/credential_provider/extension/os_service_manager.h"
#include "chrome/credential_provider/extension/service.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "chrome/credential_provider/gaiacp/experiments_fetcher.h"
#include "chrome/credential_provider/gaiacp/experiments_manager.h"
#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"

using credential_provider::GetGlobalFlagOrDefault;
using credential_provider::kRegEnableVerboseLogging;

// Register all tasks for ESA with the TaskManager.
void RegisterAllTasks() {
  // Task to fetch experiments for all GCPW users. Keeping this as the first
  // task so that latest version of experiments is available to all of the other
  // tasks.
  if (credential_provider::ExperimentsManager::Get()->ExperimentsEnabled()) {
    credential_provider::extension::TaskManager::Get()->RegisterTask(
        "FetchExperiments", credential_provider::ExperimentsFetcher::
                                GetFetchExperimentsTaskCreator());
  }

  // Task to fetch Cloud policies for all GCPW users.
  if (credential_provider::UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    credential_provider::extension::TaskManager::Get()->RegisterTask(
        "FetchCloudPolicies", credential_provider::UserPoliciesManager::
                                  GetFetchPoliciesTaskCreator());
  }

  // Task to Upload device details.
  if (credential_provider::GemDeviceDetailsManager::Get()
          ->UploadDeviceDetailsFromEsaFeatureEnabled()) {
    credential_provider::extension::TaskManager::Get()->RegisterTask(
        "UploadDeviceDetails", credential_provider::GemDeviceDetailsManager::
                                   UploadDeviceDetailsTaskCreator());

    // Task to Upload app data.
    if (credential_provider::AppInventoryManager::Get()
            ->UploadAppInventoryFromEsaFeatureEnabled()) {
      credential_provider::extension::TaskManager::Get()->RegisterTask(
          "UploadAppInventory", credential_provider::AppInventoryManager::
                                    UploadAppInventoryTaskCreator());
    }
  }
}

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

  // Set the event logging source and category for GCPW Extension.
  logging::SetEventSource("GCPW", GCPW_EXTENSION_CATEGORY, MSG_LOG_MESSAGE);

  if (GetGlobalFlagOrDefault(kRegEnableVerboseLogging, 0))
    logging::SetMinLogLevel(logging::LOGGING_VERBOSE);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());

  RegisterAllTasks();

  return credential_provider::extension::Service::Get()->Run();
}
