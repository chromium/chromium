// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/scanner_controller.h"

#include <stdlib.h>
#include <time.h>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/os/shutdown_watchdog.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

namespace {

// The maximal allowed time to run the scanner (5 minutes).
const uint32_t kWatchdogTimeoutInSeconds = 5 * 60;

ResultCode GetResultCodeFromFoundUws(const std::vector<UwSId>& found_uws) {
  for (UwSId uws_id : found_uws) {
    if (!PUPData::IsKnownPUP(uws_id))
      return RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS;
  }

  // Removal has precedence over other states.
  if (PUPData::HasFlaggedPUP(found_uws, &PUPData::HasRemovalFlag)) {
    return RESULT_CODE_SUCCESS;
  }

  if (PUPData::HasFlaggedPUP(found_uws, &PUPData::HasConfirmedUwSFlag)) {
    return RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY;
  }

  if (PUPData::HasFlaggedPUP(found_uws, &PUPData::HasReportOnlyFlag)) {
    return RESULT_CODE_REPORT_ONLY_PUPS_FOUND;
  }

  DCHECK(found_uws.empty());
  return RESULT_CODE_NO_PUPS_FOUND;
}

}  // namespace

ScannerController::~ScannerController() = default;

int ScannerController::ScanOnly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure the scanning process gets completed in a reasonable amount of
  // time, otherwise log it and terminate the process.
  base::TimeDelta watchdog_timeout =
      base::TimeDelta::FromSeconds(watchdog_timeout_in_seconds_);

  Settings* settings = Settings::GetInstance();
  if (settings->scanning_timeout_overridden())
    watchdog_timeout = settings->scanning_timeout();

  std::unique_ptr<ShutdownWatchdog> watchdog;
  if (!watchdog_timeout.is_zero()) {
    watchdog = std::make_unique<ShutdownWatchdog>(
        watchdog_timeout,
        base::BindOnce(&ScannerController::WatchdogTimeoutCallback,
                       base::Unretained(this)));
    watchdog->Arm();
  }

  std::vector<int> keys_of_paths_to_explore = {
      base::DIR_USER_DESKTOP,      base::DIR_COMMON_DESKTOP,
      base::DIR_USER_QUICK_LAUNCH, base::DIR_START_MENU,
      base::DIR_COMMON_START_MENU, base::DIR_TASKBAR_PINS};

  // TODO(proberge): We can move the following code to live inside
  // FindAndParseChromeShortcutsInFoldersAsync so it can be shared with
  // SystemReportComponent.
  std::vector<base::FilePath> paths_to_explore;
  for (int path_key : keys_of_paths_to_explore) {
    base::FilePath path;
    if (base::PathService::Get(path_key, &path))
      paths_to_explore.push_back(path);
  }

  if (shortcut_parser_) {
    std::set<base::FilePath> chrome_exe_paths;
    ListChromeExePaths(&chrome_exe_paths);
    FilePathSet chrome_exe_file_path_set;
    for (const auto& path : chrome_exe_paths)
      chrome_exe_file_path_set.Insert(path);

    shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
        paths_to_explore, chrome_exe_file_path_set,
        base::BindOnce(
            [](base::WaitableEvent* event,
               std::vector<ShortcutInformation>* shortcuts_found,
               std::vector<ShortcutInformation> parsed_shortcuts) {
              *shortcuts_found = parsed_shortcuts;
              event->Signal();
            },
            &shortcut_parsing_event_, &shortcuts_found_));
  } else {
    // If this branch executes it means the shortcut parsing is not enabled
    // on the command line.
    shortcut_parsing_event_.Signal();
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitWhenIdleClosure();
  StartScan();
  run_loop.Run();

  if (watchdog)
    watchdog->Disarm();

  DCHECK_NE(RESULT_CODE_INVALID, result_code_);
  return static_cast<int>(result_code_);
}

ScannerController::ScannerController(RegistryLogger* registry_logger,
                                     ShortcutParserAPI* shortcut_parser)
    : registry_logger_(registry_logger),
      watchdog_timeout_in_seconds_(kWatchdogTimeoutInSeconds),
      shortcut_parser_(shortcut_parser),
      shortcut_parsing_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(registry_logger);
}

void ScannerController::DoneScanning(ResultCode status,
                                     const std::vector<UwSId>& found_pups) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Shortcut parsing is done in parallel to the regular scan, if it is not
  // complete yet, wait for it to finish.
  shortcut_parsing_event_.Wait();

  UpdateScanResults(found_pups);
  if (status == RESULT_CODE_SUCCESS)
    status = GetResultCodeFromFoundUws(found_pups);
  {
    base::AutoLock lock(lock_);
    result_code_ = status;
  }

  LoggingServiceAPI* logging_service_api = LoggingServiceAPI::GetInstance();

  const base::string16 kChromeExecutableName = L"chrome.exe";
  bool has_modified_shortcuts = false;
  for (const auto& shortcut : shortcuts_found_) {
    base::FilePath target_path(shortcut.target_path);

    // If any of the returned shortcuts is pointing to a file that is not
    // chrome.exe or that contains arguments report that we found shortcuts
    // modifications.
    if (target_path.BaseName().value() != kChromeExecutableName ||
        !shortcut.command_line_arguments.empty()) {
      has_modified_shortcuts = true;
      break;
    }
  }
  logging_service_api->SetFoundModifiedChromeShortcuts(has_modified_shortcuts);

  SystemResourceUsage stats;
  if (GetSystemResourceUsage(::GetCurrentProcess(), &stats))
    logging_service_api->LogProcessInformation(SandboxType::kNonSandboxed,
                                               stats);
  std::map<SandboxType, SystemResourceUsage> sbox_process_usage =
      GetSandboxSystemResourceUsage();
  for (const auto& type_usage : sbox_process_usage) {
    LoggingServiceAPI::GetInstance()->LogProcessInformation(type_usage.first,
                                                            type_usage.second);
  }

  logging_service_api->SetExitCode(status);
  logging_service_api->MaybeSaveLogsToFile(L"");
  logging_service_api->SendLogsToSafeBrowsing(
      base::BindRepeating(&ScannerController::LogsUploadComplete,
                          base::Unretained(this)),
      registry_logger_);
}

void ScannerController::UpdateScanResults(
    const std::vector<UwSId>& found_pups) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Log which PUPs were found.
  registry_logger_->RecordFoundPUPs(found_pups);

  ResultCode result = GetResultCodeFromFoundUws(found_pups);
  {
    base::AutoLock lock(lock_);
    result_code_ = result;
  }
}

int ScannerController::WatchdogTimeoutCallback() {
  ResultCode result_code;
  {
    base::AutoLock lock(lock_);
    result_code = result_code_;
  }

  int watchdog_result_code =
      result_code == RESULT_CODE_SUCCESS
          ? RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS
          : RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS;

  HandleWatchdogTimeout(watchdog_result_code);
  return watchdog_result_code;
}

void ScannerController::HandleWatchdogTimeout(ResultCode result_code) {
  {
    base::AutoLock lock(lock_);
    result_code_ = result_code;
  }
  registry_logger_->WriteExitCode(result_code);
  registry_logger_->WriteEndTime();
}

void ScannerController::LogsUploadComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(quit_closure_).Run();
}

}  // namespace chrome_cleaner
