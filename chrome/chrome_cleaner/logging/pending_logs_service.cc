// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/pending_logs_service.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop_current.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

bool PendingLogsService::retrying_ = false;

// static.
base::string16 PendingLogsService::LogsUploadRetryTaskName(
    const base::string16& product_shortname) {
  return product_shortname + L" logs upload retry";
}

// static.
void PendingLogsService::ScheduleLogsUploadTask(
    const base::string16& product_shortname,
    const ChromeCleanerReport& chrome_cleaner_report,
    base::FilePath* log_file,
    RegistryLogger* registry_logger) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(log_file);
  DCHECK(registry_logger);
  // This can happen when we fail while retrying. The logging service is not
  // aware of this state and will call us again. Simply ignore the call.
  if (retrying_)
    return;

  std::string chrome_cleaner_report_string;
  if (!chrome_cleaner_report.SerializeToString(&chrome_cleaner_report_string)) {
    PLOG(ERROR) << "SerializeToString failed!";
    return;
  }

  base::FilePath product_app_data_path;
  base::FilePath temp_file_path;
  if (!GetAppDataProductDirectory(&product_app_data_path) ||
      !CreateTemporaryFileInDir(product_app_data_path, &temp_file_path)) {
    PLOG(ERROR) << "Create a temporary log file failed!";
    return;
  }

  // To get rid of the temporary file if we are not going to use it.
  base::ScopedClosureRunner delete_file_closure(base::BindRepeating(
      IgnoreResult(&base::DeleteFile), temp_file_path, false));

  if (base::WriteFile(temp_file_path, chrome_cleaner_report_string.c_str(),
                      chrome_cleaner_report_string.size()) <= 0) {
    PLOG(ERROR) << "Failed to write logging report to "
                << SanitizePath(temp_file_path);
    return;
  }

  // AppendLogFilePath already logs sufficiently on failures.
  if (!registry_logger->AppendLogFilePath(temp_file_path))
    return;

  base::CommandLine scheduled_command_line(
      PreFetchedPaths::GetInstance()->GetExecutablePath());
  scheduled_command_line.AppendSwitch(kUploadLogFileSwitch);
  // Propage the cleanup id for the current process, so we can identify the
  // corresponding pending logs.
  scheduled_command_line.AppendSwitchASCII(
      kCleanupIdSwitch, Settings::GetInstance()->cleanup_id());
  scheduled_command_line.AppendArguments(
      *base::CommandLine::ForCurrentProcess(), false);
  std::unique_ptr<TaskScheduler> task_scheduler(
      TaskScheduler::CreateInstance());
  if (!task_scheduler->RegisterTask(
          LogsUploadRetryTaskName(product_shortname).c_str(),
          /*task_description=*/L"", scheduled_command_line,
          TaskScheduler::TRIGGER_TYPE_EVERY_SIX_HOURS, false)) {
    LOG(ERROR) << "Failed to register logs upload retry task.";
    registry_logger->RemoveLogFilePath(temp_file_path);
    return;
  }
  // TODO(csharp): when we add support to upload pending logs in regular
  // unscheduled runs, this should move up before the scheduling.
  // Prevent the file deletion now that we confirmed it will be used.
  delete_file_closure.Release().Reset();
  *log_file = temp_file_path;
}

// static.
void PendingLogsService::ClearPendingLogFile(
    const base::string16& product_shortname,
    const base::FilePath& log_file,
    RegistryLogger* registry_logger) {
  DCHECK(registry_logger);
  // RemoveLogFilePath returns false when there are no log files left in the
  // registry, in which case we must delete the scheduled task too.
  if (!registry_logger->RemoveLogFilePath(log_file)) {
    std::unique_ptr<TaskScheduler> task_scheduler(
        TaskScheduler::CreateInstance());
    if (!task_scheduler->DeleteTask(
            LogsUploadRetryTaskName(product_shortname).c_str()))
      LOG(ERROR) << "Failed to delete logs upload retry task.";
  }

  if (!base::DeleteFile(log_file, false))
    LOG(ERROR) << "Failed to delete '" << SanitizePath(log_file) << "'.";
}

PendingLogsService::PendingLogsService() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
}

PendingLogsService::~PendingLogsService() = default;

void PendingLogsService::RetryNextPendingLogsUpload(
    const base::string16& product_shortname,
    base::OnceCallback<void(bool)> done_callback,
    RegistryLogger* registry_logger) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(registry_logger);
  DCHECK(!retrying_);
  retrying_ = true;

  LoggingServiceAPI* logging_service = LoggingServiceAPI::GetInstance();

  // TODO(csharp): Loop through all the pending log files in one shot instead of
  // having to wait for another scheduled task interval for the next one.
  registry_logger->GetNextLogFilePath(&log_file_);
  if (log_file_.empty()) {
    LOG(ERROR) << "Got no log file path from registry";
    logging_service->SetExitCode(RESULT_CODE_FAILED_TO_READ_UPLOAD_LOGS_FILE);
  } else if (!logging_service->ReadContentFromFile(log_file_)) {
    LOG(ERROR) << "Error reading log content from file";
    ClearPendingLogFile(product_shortname, log_file_, registry_logger);
    log_file_.clear();

    // Send a plain log with just the proper result code in case it can work.
    logging_service->SetExitCode(RESULT_CODE_FAILED_TO_READ_UPLOAD_LOGS_FILE);
  } else {
    // Add one more log before we upload, to specify that we are sending this
    // log from a retry task. TODO(csharp): Maybe add this as a protobuf field.
    LOG(INFO) << "Uploading persisted pending logs.";
  }

  done_callback_ = std::move(done_callback);
  logging_service->SendLogsToSafeBrowsing(
      base::BindRepeating(&PendingLogsService::UploadResultCallback,
                          base::Unretained(this), product_shortname,
                          registry_logger),
      registry_logger);
}

void PendingLogsService::UploadResultCallback(
    const base::string16& product_shortname,
    RegistryLogger* registry_logger,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(retrying_);
  if (success && !log_file_.empty())
    ClearPendingLogFile(product_shortname, log_file_, registry_logger);

  log_file_.clear();
  retrying_ = false;
  std::move(done_callback_).Run(success);
}

}  // namespace chrome_cleaner
