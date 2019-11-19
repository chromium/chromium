// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/main_controller.h"

#include <shellapi.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/components/component_manager.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/engines/common/engine_digest_verifier.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/scoped_timed_task_logger.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/os/rebooter.h"
#include "chrome/chrome_cleaner/os/rebooter_api.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/shutdown_watchdog.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/ui/chrome_proxy_main_dialog.h"
#include "chrome/chrome_cleaner/ui/silent_main_dialog.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

namespace {
const base::TimeDelta kUserResponseWatchdogTimeout =
    base::TimeDelta::FromHours(2);
const base::TimeDelta kCleanerWatchdogTimeout = base::TimeDelta::FromHours(2);

// Log memory usage, CPU usage and various IO counters.
void LogSystemResourceUsage() {
  base::ProcessHandle process_handle = base::GetCurrentProcessHandle();
  SystemResourceUsage stats;
  if (!GetSystemResourceUsage(process_handle, &stats))
    return;

  LoggingServiceAPI::GetInstance()->LogProcessInformation(
      SandboxType::kNonSandboxed, stats);

  // TODO(veranika): remove raw logging of resource usage.
  LOG(INFO) << "User time: '" << stats.user_time.InSeconds() << "' seconds.";
  LOG(INFO) << "Kernel time: '" << stats.kernel_time.InSeconds()
            << "' seconds.";

  // Log memory usage.
  LOG(INFO) << "Peak memory used: '" << stats.peak_working_set_size / 1024
            << "' KB.";

  // Log IO counters.
  LOG(INFO) << "IO counters: "
            << "ReadOperationCount = '" << stats.io_counters.ReadOperationCount
            << "', WriteOperationCount = '"
            << stats.io_counters.WriteOperationCount
            << "', OtherOperationCount = '"
            << stats.io_counters.OtherOperationCount
            << "', ReadTransferCount = '" << stats.io_counters.ReadTransferCount
            << "', WriteTransferCount = '"
            << stats.io_counters.WriteTransferCount
            << "', OtherTransferCount = '"
            << stats.io_counters.OtherTransferCount << "'";

  std::map<SandboxType, SystemResourceUsage> sbox_process_usage =
      GetSandboxSystemResourceUsage();
  for (const auto& type_usage : sbox_process_usage) {
    LoggingServiceAPI::GetInstance()->LogProcessInformation(type_usage.first,
                                                            type_usage.second);
  }
}

}  // namespace

class MainController::ChromePromptConnectionErrorHandler
    : public ChromePromptIPC::ErrorHandler {
 public:
  ChromePromptConnectionErrorHandler(
      base::WeakPtr<MainController> main_controller,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ChromePromptConnectionErrorHandler() override;

  void OnConnectionClosed() override;
  void OnConnectionClosedAfterDone() override;

 private:
  // Task runner where IPC connection error handling tasks must be posted to.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Pointer to the main controller that created this object that will
  // respond to connection error. The weak pointer will be invalidated once
  // the main controller is destroyed, so we don't try to access it from
  // OnConnectionClosed().
  base::WeakPtr<MainController> main_controller_;
};

MainController::MainController(RebooterAPI* rebooter,
                               RegistryLogger* registry_logger,
                               ChromePromptIPC* chrome_prompt_ipc)
    : user_response_watchdog_timeout_(kUserResponseWatchdogTimeout),
      cleaning_watchdog_timeout_(kCleanerWatchdogTimeout),
      rebooter_(rebooter),
      component_manager_(this),
      result_code_(RESULT_CODE_INVALID),
      registry_logger_(registry_logger) {
  Settings* settings = Settings::GetInstance();
  ExecutionMode execution_mode = settings->execution_mode();
  DCHECK(execution_mode == ExecutionMode::kScanning ||
         execution_mode == ExecutionMode::kCleanup);

  // The IPC object should only be available if execution mode is kScanning.
  DCHECK_EQ(chrome_prompt_ipc != nullptr,
            execution_mode == ExecutionMode::kScanning);

  LoggingServiceAPI::GetInstance()->EnableUploads(
      settings->logs_upload_allowed(), registry_logger_);

  if (execution_mode == ExecutionMode::kScanning) {
    // Connection handler object is leaked since it must outlive the
    // ChromePromptIPC object. The weak pointer to the main controller is
    // invalidated on object destruction, so it's not accessed once this
    // main controller object no longer exists.
    chrome_prompt_ipc->Initialize(new ChromePromptConnectionErrorHandler(
        weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get()));
    main_dialog_.reset(new ChromeProxyMainDialog(this, chrome_prompt_ipc));
  } else if (execution_mode == ExecutionMode::kCleanup) {
    main_dialog_.reset(new SilentMainDialog(this));
  } else {
    NOTREACHED();
  }

  if (settings->scanning_timeout_overridden())
    scanning_watchdog_timeout_ = settings->scanning_timeout();
  if (settings->user_response_timeout_overridden())
    user_response_watchdog_timeout_ = settings->user_response_timeout();
  if (settings->cleaning_timeout_overridden())
    cleaning_watchdog_timeout_ = settings->cleaning_timeout();
}

SandboxConnectionErrorCallback
MainController::GetSandboxConnectionErrorCallback() {
  // Returns a callback bound with the current task runner and the weak pointer
  // of main controller.
  return base::BindRepeating(MainController::CallSandboxConnectionClosed,
                             base::SequencedTaskRunnerHandle::Get(),
                             weak_factory_.GetWeakPtr());
}

MainDialogAPI* MainController::main_dialog() {
  return main_dialog_.get();
}

MainController::~MainController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(joenotcharles): Cleanup RunUntilIdle usage in loops.
  CHECK(!base::RunLoop::IsRunningOnCurrentThread());
  if (engine_facade_) {
    while (!cleaner()->IsCompletelyDone() || !scanner()->IsCompletelyDone())
      base::RunLoop().RunUntilIdle();
  }

  // We must make sure to close the components before we destroy the manager.
  component_manager_.CloseAllComponents(result_code_);
}

void MainController::AddComponent(std::unique_ptr<ComponentAPI> component) {
  component_manager_.AddComponent(std::move(component));
}

void MainController::SetEngineFacade(EngineFacadeInterface* engine_facade) {
  engine_facade_ = engine_facade;
  if (!Settings::GetInstance()->scanning_timeout_overridden()) {
    scanning_watchdog_timeout_ = engine_facade_->GetScanningWatchdogTimeout();
  }
}

Scanner* MainController::scanner() {
  DCHECK(engine_facade_);
  return engine_facade_->GetScanner();
}

Cleaner* MainController::cleaner() {
  DCHECK(engine_facade_);
  return engine_facade_->GetCleaner();
}

ResultCode MainController::ScanAndClean() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Enable watchdog for non-legacy execution modes. If this process is in
  // scanning execution mode, then we will set a timeout for the scanner, that
  // will be disarmed once scan has finished. If this is in cleanup mode, the
  // timeout will only be disarmed once cleanup has finished.
  if (Settings::GetInstance()->execution_mode() == ExecutionMode::kScanning &&
      !scanning_watchdog_timeout_.is_zero()) {
    watchdog_ = std::make_unique<ShutdownWatchdog>(
        scanning_watchdog_timeout_,
        base::BindOnce(&MainController::WatchdogTimeoutCallback,
                       base::Unretained(this), TimedOutStage::kScanning));
    watchdog_->Arm();
  } else if (Settings::GetInstance()->execution_mode() ==
                 ExecutionMode::kCleanup &&
             !cleaning_watchdog_timeout_.is_zero()) {
    watchdog_ = std::make_unique<ShutdownWatchdog>(
        cleaning_watchdog_timeout_,
        base::BindOnce(&MainController::WatchdogTimeoutCallback,
                       base::Unretained(this), TimedOutStage::kCleaning));
    watchdog_->Arm();
  }

  if (!main_dialog()->Create())
    return RESULT_CODE_FAILED;

  scan_start_time_ = base::Time::Now();
  component_manager_.PreScan();

  base::RunLoop run_loop;
  DCHECK(!quit_closure_);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  // By this point, result_code_ should have been set.
  DCHECK_NE(result_code_, RESULT_CODE_INVALID);
  return result_code_;
}

ResultCode MainController::ValidateCleanup() {
  ScopedTimedTaskLogger scoped_timed_task_logger("ValidateCleanup");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAdminRights())
      << "Implementing scan without admin rights? Please validate that the "
         "Event code at startup works correctly.";

  scanner()->Start(
      base::BindRepeating(&MainController::FoundUwSCallback,
                          base::Unretained(this)),
      base::BindOnce(&MainController::DoneValidating, base::Unretained(this)));

  base::RunLoop run_loop;
  DCHECK(!quit_closure_);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  // |result_code_| was set by DoneValidating.
  return result_code_;
}

MainController::ChromePromptConnectionErrorHandler::
    ChromePromptConnectionErrorHandler(
        base::WeakPtr<MainController> main_controller,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner), main_controller_(std::move(main_controller)) {}

MainController::ChromePromptConnectionErrorHandler::
    ~ChromePromptConnectionErrorHandler() = default;

void MainController::ChromePromptConnectionErrorHandler::OnConnectionClosed() {
  // Posts an OnConnectionClosed() task to the main controller's thread,
  // if the main controller still exists. If the weak pointer to the main
  // controller has been invalidated, this will be a no-op.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MainController::OnChromePromptConnectionClosed,
                                main_controller_));
}

void MainController::ChromePromptConnectionErrorHandler::
    OnConnectionClosedAfterDone() {
  // No action required if the pipe is disconnected when it's no longer needed.
  // Changing this method to access the |main_controller_| will require
  // changing the error handler's lifetime.
}

void MainController::FoundUwSCallback(UwSId pup_id) {
  if (PUPData::HasRemovalFlag(PUPData::GetPUP(pup_id)->signature().flags))
    removable_uws_found_ = true;
}

void MainController::DoneScanning(ResultCode result_code,
                                  const std::vector<UwSId>& found_pups) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  found_pups_ = found_pups;
  if (result_code != RESULT_CODE_SUCCESS)
    result_code_ = result_code;
  component_manager_.PostScan(found_pups);
}

void MainController::DoneValidating(ResultCode result_code,
                                    const std::vector<UwSId>& found_pups) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get PUPs to remove.
  std::vector<UwSId> found_pups_to_remove;
  PUPData::ChoosePUPs(found_pups, &PUPData::HasRemovalFlag,
                      &found_pups_to_remove);

  if (result_code == RESULT_CODE_SUCCESS && !found_pups_to_remove.empty()) {
    cleaner()->StartPostReboot(
        found_pups_to_remove,
        base::BindOnce(&MainController::DoneCleanupValidation,
                       base::Unretained(this)));
  } else {
    DoneCleanupValidation(result_code);
  }
}

void MainController::DoneCleanupValidation(ResultCode validation_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(Rebooter::IsPostReboot());

  // If scanner didn't find removable uws during the post-reboot run,
  // |validation_code| is its return code. Otherwise, it's the cleaner's return
  // code. Confused? Me too!

  if (validation_code == RESULT_CODE_SUCCESS ||
      validation_code == RESULT_CODE_NO_PUPS_FOUND ||
      validation_code == RESULT_CODE_REPORT_ONLY_PUPS_FOUND) {
    result_code_ = RESULT_CODE_POST_REBOOT_SUCCESS;
  } else {
    // TODO(veranika): This hides the actual error code. Post-reboot-specific
    // exit codes should be deprecated.
    LOG(ERROR) << "CleanupValidation failed: " << validation_code;
    result_code_ = RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED;
  }

  rebooter_->UnregisterPostRebootRun();

  component_manager_.PostValidation(result_code_);
  // Unlike most of the ComponentManager API, this call is synchronous, so we
  // don't need to wait for a ComponentManagerDelegate API call to be made
  // before continuing, and we can simply quit here. :-)
  Quit();
}

void MainController::OnChromePromptConnectionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scanner()->Stop();

  // There's no need to stop the cleaner since the Chrome prompt connection is
  // already closed in the cleaning phase.
  DCHECK(cleaner()->IsCompletelyDone());

  result_code_ = RESULT_CODE_CHROME_PROMPT_IPC_DISCONNECTED_TOO_SOON;
  Quit();
}

// static
// Posts a task with the weak pointer of main controller to report error.
void MainController::CallSandboxConnectionClosed(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<MainController> main_controller,
    SandboxType sandbox_type) {
  // Make sure the error handler runs on the correct thread.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MainController::OnSandboxConnectionClosed,
                                main_controller, sandbox_type));
}

void MainController::OnSandboxConnectionClosed(SandboxType sandbox_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResultCode result_code = GetResultCodeForSandboxConnectionError(sandbox_type);
  WriteEarlyExitMetrics(result_code);
  EarlyExit(result_code);
}

void MainController::CleanupDone(ResultCode clean_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  result_code_ = clean_result;
  component_manager_.PostCleanup(result_code_, rebooter_);
}

void MainController::PreScanDone() {
  scanner()->Start(
      base::BindRepeating(&MainController::FoundUwSCallback,
                          base::Unretained(this)),
      base::BindOnce(&MainController::DoneScanning, base::Unretained(this)));
}

void MainController::PostScanDone() {
  DCHECK(!scan_start_time_.is_null());
  finished_scanning_ = true;
  base::TimeDelta scan_time = base::Time::Now() - scan_start_time_;
  LOG(INFO) << "Scanning took '" << scan_time.InSeconds() << "' seconds.";

  // Split PUPs into report and remove vectors.
  std::vector<UwSId> found_pups_to_remove;
  PUPData::ChoosePUPs(found_pups_, &PUPData::HasRemovalFlag,
                      &found_pups_to_remove);
  bool report_only_found =
      PUPData::HasFlaggedPUP(found_pups_, &PUPData::HasReportOnlyFlag);
  bool malicious_found =
      PUPData::HasFlaggedPUP(found_pups_, &PUPData::HasConfirmedUwSFlag);

  ResultCode fallback_logs_result_code = RESULT_CODE_INVALID;
  bool removable_pups_found = false;
  if (result_code_ != RESULT_CODE_INVALID) {
    // The result code is set before scan is done only if there was an error.
    // Do not offer cleanup.
    fallback_logs_result_code = result_code_;
  } else if (found_pups_to_remove.empty()) {
    if (malicious_found) {
      result_code_ = RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY;
    } else if (report_only_found) {
      result_code_ = RESULT_CODE_REPORT_ONLY_PUPS_FOUND;
    } else {
      result_code_ = RESULT_CODE_NO_PUPS_FOUND;
    }
    fallback_logs_result_code = result_code_;
  } else {
    if (!cleaner()->CanClean(found_pups_to_remove)) {
      LOG(ERROR) << "GetCleanupRequirements failure, skip cleanup.";
      result_code_ = RESULT_CODE_CLEANUP_REQUIREMENTS_FAILED;
      fallback_logs_result_code = result_code_;
    } else {
      removable_pups_found = true;
      found_pups_ = found_pups_to_remove;
    }
  }

  // Stop the watchdog set for the scanning phase if this process is running in
  // scanning mode.
  if (Settings::GetInstance()->execution_mode() == ExecutionMode::kScanning &&
      watchdog_) {
    watchdog_->Disarm();
    watchdog_.reset();
  }

  if (!removable_pups_found) {
    main_dialog()->NoPUPsFound();
  } else {
    if (Settings::GetInstance()->execution_mode() == ExecutionMode::kScanning &&
        !user_response_watchdog_timeout_.is_zero()) {
      // Sets a timeout for the user response. This will be disarmed once we
      // receive a response from Chrome.
      watchdog_ = std::make_unique<ShutdownWatchdog>(
          user_response_watchdog_timeout_,
          base::BindOnce(&MainController::WatchdogTimeoutCallback,
                         base::Unretained(this),
                         TimedOutStage::kWaitingForPrompt));
      watchdog_->Arm();
    }

    main_dialog()->ConfirmCleanupIfNeeded(found_pups_to_remove,
                                          GetDigestVerifier());
    fallback_logs_result_code = RESULT_CODE_FAILED_TO_START_CLEANUP;
  }

  // Now that the user had the opportunity to opt-out of logs upload, schedule a
  // fall back logs upload in case something goes wrong later on.
  LoggingServiceAPI::GetInstance()->ScheduleFallbackLogsUpload(
      registry_logger_, fallback_logs_result_code);
}

void MainController::PreCleanupDone() {
  cleaner()->Start(found_pups_, base::BindOnce(&MainController::CleanupDone,
                                               base::Unretained(this)));
}

void MainController::PostCleanupDone() {
  // Don't call CleanupDone when the user already canceled.
  if (result_code_ == RESULT_CODE_CANCELED) {
    Quit();
    return;
  }

  // Disable the watchdog that was set when the process in cleanup mode
  // started, which includes scanning and cleaning.
  Settings* settings = Settings::GetInstance();
  if (settings->execution_mode() == ExecutionMode::kCleanup && watchdog_) {
    watchdog_->Disarm();
    watchdog_.reset();
  }

  DCHECK(!clean_start_time_.is_null());
  finished_cleaning_ = true;
  base::TimeDelta clean_time = base::Time::Now() - clean_start_time_;
  LOG(INFO) << "Cleaning took '" << clean_time.InSeconds() << "' seconds.";

  // TODO(joenotcharles): Add proper tests for the main controller's interaction
  // with the rebooter class
  if (result_code_ == RESULT_CODE_PENDING_REBOOT &&
      settings->execution_mode() != ExecutionMode::kScanning) {
    // TODO(csharp): Find a way to handle Rebooter errors.
    // We currently ignore failure to run again after restart, as long as a
    // restart happen, at least we can hope that the job will be done, we just
    // won't be able to confirm it.
    if (!rebooter_->RegisterPostRebootRun(
            base::CommandLine::ForCurrentProcess(), settings->cleanup_id(),
            settings->execution_mode(),
            LoggingServiceAPI::GetInstance()->uploads_enabled()))
      PLOG(ERROR) << "Failed to RegisterPostRebootRun().";
  }

  main_dialog()->CleanupDone(result_code_);
}

void MainController::AcceptedCleanup(bool accepted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Disable the watchdog that was set when the user was prompted.
  if (Settings::GetInstance()->execution_mode() == ExecutionMode::kScanning &&
      watchdog_) {
    watchdog_->Disarm();
    watchdog_.reset();
  }

  if (!accepted) {
    result_code_ = RESULT_CODE_CLEANUP_PROMPT_DENIED;
    Quit();
    return;
  }

  registry_logger_->WriteExitCode(RESULT_CODE_PENDING);
  UploadLogs(L"-intermediate", false);

  // Resets the global indication that a cleanup completed. This will be
  // set again once the current cleanup completes and the information will
  // be used by Chrome to reset settings for the profile that accepted the
  // prompt.
  registry_logger_->ResetCompletedCleanup();

  clean_start_time_ = base::Time::Now();
  component_manager_.PreCleanup();
}

void MainController::OnClose() {
  // In case the window get closed while scanning/cleaning.
  scanner()->Stop();
  cleaner()->Stop();
  if (result_code_ == RESULT_CODE_INVALID)
    result_code_ = RESULT_CODE_CANCELED;
  if (!scan_start_time_.is_null() && !finished_scanning_) {
    base::TimeDelta scan_time = base::Time::Now() - scan_start_time_;
    LOG(INFO) << "Scanning was canceled after '" << scan_time.InSeconds()
              << "' seconds spent in the scanning phase.";
  }
  if (!clean_start_time_.is_null() && !finished_cleaning_) {
    base::TimeDelta clean_time = base::Time::Now() - clean_start_time_;
    LOG(INFO) << "Cleaning was canceled after '" << clean_time.InSeconds()
              << "' seconds spent in the cleaning phase.";
  }

  LogSystemResourceUsage();

  LoggingServiceAPI* logging_service = LoggingServiceAPI::GetInstance();
  logging_service->SetExitCode(result_code_);

  UploadLogs(base::string16(), true);

  // This must be called after we uploaded logs to make sure none is added
  // after the user had a chance to opt-out.
  component_manager_.CloseAllComponents(result_code_);
}

void MainController::UploadLogs(const base::string16& tag,
                                bool quit_when_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoggingServiceAPI* logging_service = LoggingServiceAPI::GetInstance();

  DCHECK(logs_upload_complete_.find(tag) == logs_upload_complete_.end())
      << "tag is not unique";
  logs_upload_complete_[tag] = false;

  if (quit_when_done)
    quit_when_logs_upload_complete_ = true;

  logging_service->MaybeSaveLogsToFile(tag);

  if (Settings::GetInstance()->logs_upload_allowed()) {
    LoggingServiceAPI* logging_service = LoggingServiceAPI::GetInstance();
    logging_service->SendLogsToSafeBrowsing(
        base::BindRepeating(&MainController::LogsUploadComplete,
                            base::Unretained(this), tag),
        registry_logger_);
  } else {
    LogsUploadComplete(tag, false);  // false since no logs were uploaded.
  }
}

void MainController::Quit() {
  main_dialog()->Close();
}

void MainController::WriteEarlyExitMetrics(ResultCode exit_code) {
  registry_logger_->WriteExitCode(exit_code);
  registry_logger_->WriteEndTime();
}

int MainController::WatchdogTimeoutCallback(TimedOutStage timed_out_stage) {
  ResultCode exit_code = RESULT_CODE_INVALID;
  switch (timed_out_stage) {
    case TimedOutStage::kScanning:
      exit_code = removable_uws_found_
                      ? RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS
                      : RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS;
      break;

    case TimedOutStage::kWaitingForPrompt:
      exit_code = RESULT_CODE_WATCHDOG_TIMEOUT_WAITING_FOR_PROMPT_RESPONSE;
      break;

    case TimedOutStage::kCleaning:
      exit_code = RESULT_CODE_WATCHDOG_TIMEOUT_CLEANING;
      break;

    default:
      NOTREACHED();
  }

  WriteEarlyExitMetrics(exit_code);

  return exit_code;
}

void MainController::LogsUploadComplete(const base::string16& tag,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  registry_logger_->AppendLogUploadResult(success);

  std::map<base::string16, bool>::iterator it = logs_upload_complete_.find(tag);
  DCHECK(it != logs_upload_complete_.end());
  it->second = true;

  if (quit_when_logs_upload_complete_) {
    for (const std::pair<base::string16, bool>& entry : logs_upload_complete_) {
      if (!entry.second) {
        LOG(INFO) << "Waiting for the upload of logs with tag \"" << entry.first
                  << "\" to complete before exiting";
        return;
      }
    }
    if (quit_closure_)
      std::move(quit_closure_).Run();
  } else {
    // When we complete a logs upload, and it's not the last one (identified
    // by |quit_when_logs_upload_complete_|), we schedule a fallback logs upload
    // in case the process dies before we get to the next log upload.
    LoggingServiceAPI::GetInstance()->ScheduleFallbackLogsUpload(
        registry_logger_, RESULT_CODE_FAILED_TO_COMPLETE_CLEANUP);
  }
}

}  // namespace chrome_cleaner
