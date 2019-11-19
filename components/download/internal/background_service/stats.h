// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STATS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STATS_H_

#include "base/files/file.h"
#include "base/optional.h"
#include "components/download/internal/background_service/controller.h"
#include "components/download/internal/background_service/download_blockage_status.h"
#include "components/download/internal/background_service/driver_entry.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/task/download_task_types.h"

namespace download {

struct StartupStatus;

namespace stats {

// Please follow the following rules for all enums:
// 1. Keep them in sync with the corresponding entry in enums.xml.
// 2. Treat them as append only.
// 3. Do not remove any enums.  Only mark them as deprecated.

// Enum used to track download service start up result and failure reasons.
// Most of the fields should map to StartupStatus.
// Failure reasons are not mutually exclusive.
enum class StartUpResult {
  // Download service successfully started.
  SUCCESS = 0,

  // Download service start up failed.
  FAILURE = 1,

  // Download driver is not ready.
  FAILURE_REASON_DRIVER = 2,

  // Database layer failed to initialized.
  FAILURE_REASON_MODEL = 3,

  // File monitor failed to start.
  FAILURE_REASON_FILE_MONITOR = 4,

  // The count of entries for the enum.
  COUNT = 5,
};

// Enum used by UMA metrics to track which actions a Client is taking on the
// service.
enum class ServiceApiAction {
  // Represents a call to DownloadService::StartDownload.
  START_DOWNLOAD = 0,

  // Represents a call to DownloadService::PauseDownload.
  PAUSE_DOWNLOAD = 1,

  // Represents a call to DownloadService::ResumeDownload.
  RESUME_DOWNLOAD = 2,

  // Represents a call to DownloadService::CancelDownload.
  CANCEL_DOWNLOAD = 3,

  // Represents a call to DownloadService::ChangeCriteria.
  CHANGE_CRITERIA = 4,

  // The count of entries for the enum.
  COUNT = 5,
};

// Enum used by UMA metrics to tie to specific actions taken on a Model.  This
// can be used to track failure events.
enum class ModelAction {
  // Represents an attempt to initialize the Model.
  INITIALIZE = 0,

  // Represents an attempt to add an Entry to the Model.
  ADD = 1,

  // Represents an attempt to update an Entry in the Model.
  UPDATE = 2,

  // Represents an attempt to remove an Entry from the Model.
  REMOVE = 3,

  // The count of entries for the enum.
  COUNT = 4,
};

// Enum used by UMA metrics to log the status of scheduled tasks.
enum class ScheduledTaskStatus {
  // Startup failed and the task was not run.
  ABORTED_ON_FAILED_INIT = 0,

  // OnStopScheduledTask() was received before the task could be fired.
  CANCELLED_ON_STOP = 1,

  // Callback was run successfully after completion of the task.
  COMPLETED_NORMALLY = 2,

  // The count of entries for the enum.
  COUNT = 3,
};

// Enum used by UMA metrics to track various types of cleanup actions taken by
// the service.
enum class FileCleanupReason {
  // The file was deleted by the service after timeout.
  TIMEOUT = 0,

  // The database entry for the file was found not associated with any
  // registered client.
  ORPHANED = 1,

  // At startup, the file was found not being associated with any model entry or
  // driver entry.
  UNKNOWN = 2,

  // We're trying to remove all files as part of a hard recovery attempt.
  HARD_RECOVERY = 3,

  // The count of entries for the enum.
  COUNT = 4,
};

// Enum used by UMA metrics to log a type of download event that occurred in the
// Controller.
enum class DownloadEvent {
  // The Controller started a download.
  START = 0,

  // The Controller resumed a download (we assume this is a light weight
  // resumption that does not require a complete download restart).
  RESUME = 1,

  // The Controller is retrying a download (we assume this is a heavy weight
  // resumption that requires a complete download restart).
  RETRY = 2,

  // The Controller suspended an active download due to priorities, device
  // activity, or a request from the Client (see LogDownloadPauseReason).
  SUSPEND = 3,

  // The count of entries for the enum.
  COUNT = 4,
};

// Logs the results of starting up the Controller.  Will log each failure reason
// if |status| contains more than one initialization failure.
void LogControllerStartupStatus(bool in_recovery, const StartupStatus& status);

// Logs an action taken on the service API.
void LogServiceApiAction(DownloadClient client, ServiceApiAction action);

// Logs the result of a StartDownload() attempt on the service.
void LogStartDownloadResult(DownloadClient client,
                            DownloadParams::StartResult result);

// Logs recovery operations that happened when we had to move from one state
// to another on startup.
void LogRecoveryOperation(Entry::State to_state);

// Logs download completion event, download time, and the file size.
void LogDownloadCompletion(CompletionType type, uint64_t file_size_bytes);

// Logs various pause reasons for download. The reasons are not mutually
// exclusive.
void LogDownloadPauseReason(const DownloadBlockageStatus& blockage_status,
                            bool on_upload_data_received);
void LogEntryRemovedWhileWaitingForUploadResponse();

// Logs statistics about the result of a model operation.  Used to track failure
// cases.
void LogModelOperationResult(ModelAction action, bool success);

// Logs the total number of all entries, and the number of entries in each
// state after the model is initialized.
void LogEntries(std::map<Entry::State, uint32_t>& entries_count);

// Log statistics about the status of a TaskFinishedCallback.
void LogScheduledTaskStatus(DownloadTaskType task_type,
                            ScheduledTaskStatus status);

// Logs download files directory creation error.
void LogsFileDirectoryCreationError(base::File::Error error);

// Logs statistics about the reasons of a file cleanup.
void LogFileCleanupStatus(FileCleanupReason reason,
                          int succeeded_cleanups,
                          int failed_cleanups,
                          int external_cleanups);

// Logs the file life time for successfully completed download.
void LogFileLifeTime(const base::TimeDelta& file_life_time,
                     int num_cleanup_attempts);

// Logs the total disk space utilized by download files.
// This includes the total size of all the files in |file_dir|.
// This function is costly and should be called only once.
void LogFileDirDiskUtilization(int64_t total_disk_space,
                               int64_t free_disk_space,
                               int64_t files_size);

// Logs if the final download file path is different from the requested file
// path.
void LogFilePathRenamed(bool renamed);

// Logs an action the Controller takes on an active download.
void LogEntryEvent(DownloadEvent event);

// At the time of a resumption, logs which resumption attempt count this is.
void LogEntryResumptionCount(uint32_t resume_count);

// At the time of a retry, logs which retry attempt count this is.
void LogEntryRetryCount(uint32_t retry_count);

// Records whether the entry was an upload.
void LogHasUploadData(DownloadClient client, bool has_upload_data);

// Records count of reduced mode to full browser transitions requested by each
// client.
void LogDownloadClientInflatedFullBrowser(DownloadClient client);

}  // namespace stats
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STATS_H_
