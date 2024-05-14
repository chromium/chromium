// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/stats.h"

#include <map>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/download/internal/background_service/startup_status.h"
#include "components/download/public/background_service/clients.h"

namespace download {
namespace stats {
namespace {

// The maximum tracked file size in KB, larger files will fall into overflow
// bucket.
const int64_t kMaxFileSizeKB = 4 * 1024 * 1024; /* 4GB */

// Enum used by UMA metrics to track various reasons of pausing a download.
enum class PauseReason {
  // The download was paused. The reason can be anything.
  ANY = 0,

  // The download was paused due to unsatisfied device criteria.
  UNMET_DEVICE_CRITERIA = 1,

  // The download was paused by client.
  PAUSE_BY_CLIENT = 2,

  // The download was paused due to external download.
  EXTERNAL_DOWNLOAD = 3,

  // The download was paused due to navigation.
  EXTERNAL_NAVIGATION = 4,

  // The count of entries for the enum.
  COUNT = 5,
};

// Converts DownloadTaskType to histogram suffix.
// Should maps to suffix string in histograms.xml.
std::string TaskTypeToHistogramSuffix(DownloadTaskType task_type) {
  switch (task_type) {
    case DownloadTaskType::DOWNLOAD_TASK:
      return "DownloadTask";
    case DownloadTaskType::CLEANUP_TASK:
      return "CleanUpTask";
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK:
    case DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK:
      NOTREACHED_IN_MIGRATION();
      return "DownloadAutoResumptionTask";
    case DownloadTaskType::DOWNLOAD_LATER_TASK:
      NOTREACHED_IN_MIGRATION();
      return "DownloadLaterTask";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Converts FileCleanupReason to histogram suffix.
// Should maps to suffix string in histograms.xml.
std::string FileCleanupReasonToHistogramSuffix(FileCleanupReason reason) {
  switch (reason) {
    case FileCleanupReason::TIMEOUT:
      return "Timeout";
    case FileCleanupReason::ORPHANED:
      return "Orphaned";
    case FileCleanupReason::UNKNOWN:
      return "Unknown";
    case FileCleanupReason::HARD_RECOVERY:
      return "HardRecovery";
    case FileCleanupReason::COUNT:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Helper method to log the pause reason for a particular download.
void LogDownloadPauseReason(PauseReason reason, bool on_upload_data_received) {
  std::string name(on_upload_data_received
                       ? "Download.Service.OnUploadDataReceived.PauseReason"
                       : "Download.Service.PauseReason");

  base::UmaHistogramEnumeration(name, reason, PauseReason::COUNT);
}

}  // namespace

void LogControllerStartupStatus(bool in_recovery, const StartupStatus& status) {
  DCHECK(status.Complete());

  // Total counts for general success/failure rate.
  LogStartUpResult(in_recovery, status.Ok() ? StartUpResult::SUCCESS
                                            : StartUpResult::FAILURE);

  // Failure reasons.
  if (!status.driver_ok.value())
    LogStartUpResult(in_recovery, StartUpResult::FAILURE_REASON_DRIVER);
  if (!status.model_ok.value())
    LogStartUpResult(in_recovery, StartUpResult::FAILURE_REASON_MODEL);
  if (!status.file_monitor_ok.value())
    LogStartUpResult(in_recovery, StartUpResult::FAILURE_REASON_FILE_MONITOR);
}

void LogStartUpResult(bool in_recovery, StartUpResult result) {
  if (in_recovery) {
    base::UmaHistogramEnumeration("Download.Service.StartUpStatus.Recovery",
                                  result, StartUpResult::COUNT);
  } else {
    base::UmaHistogramEnumeration(
        "Download.Service.StartUpStatus.Initialization", result,
        StartUpResult::COUNT);
  }
}

void LogServiceApiAction(DownloadClient client, ServiceApiAction action) {
  // Total count for each action.
  std::string name("Download.Service.Request.ClientAction");
  base::UmaHistogramEnumeration(name, action, ServiceApiAction::COUNT);

  // Total count for each action with client suffix.
  name.append(".").append(BackgroundDownloadClientToString(client));
  base::UmaHistogramEnumeration(name, action, ServiceApiAction::COUNT);
}

void LogStartDownloadResult(DownloadClient client,
                            DownloadParams::StartResult result) {
  // Total count for each start result.
  std::string name("Download.Service.Request.StartResult");
  base::UmaHistogramEnumeration(name, result,
                                DownloadParams::StartResult::COUNT);

  // Total count for each client result with client suffix.
  name.append(".").append(BackgroundDownloadClientToString(client));
  base::UmaHistogramEnumeration(name, result,
                                DownloadParams::StartResult::COUNT);
}

void LogDownloadCompletion(DownloadClient client,
                           CompletionType type,
                           uint64_t file_size_bytes) {
  // Records completion type.
  UMA_HISTOGRAM_ENUMERATION("Download.Service.Finish.Type", type,
                            CompletionType::COUNT);

  // Records the file size.
  std::string name("Download.Service.Complete.FileSize.");
  name.append(BackgroundDownloadClientToString(client));
  uint64_t file_size_kb = file_size_bytes / 1024;
  base::UmaHistogramCustomCounts(name, static_cast<int>(file_size_kb), 1,
                                 kMaxFileSizeKB, 50);
}

void LogDownloadPauseReason(const DownloadBlockageStatus& blockage_status,
                            bool currently_in_progress) {
  LogDownloadPauseReason(PauseReason::ANY, currently_in_progress);

  if (blockage_status.blocked_by_criteria)
    LogDownloadPauseReason(PauseReason::UNMET_DEVICE_CRITERIA,
                           currently_in_progress);

  if (blockage_status.entry_not_active)
    LogDownloadPauseReason(PauseReason::PAUSE_BY_CLIENT, currently_in_progress);

  if (blockage_status.blocked_by_navigation)
    LogDownloadPauseReason(PauseReason::EXTERNAL_NAVIGATION,
                           currently_in_progress);

  if (blockage_status.blocked_by_downloads)
    LogDownloadPauseReason(PauseReason::EXTERNAL_DOWNLOAD,
                           currently_in_progress);
}

void LogScheduledTaskStatus(DownloadTaskType task_type,
                            ScheduledTaskStatus status) {
  std::string name("Download.Service.TaskScheduler.Status");
  base::UmaHistogramEnumeration(name, status, ScheduledTaskStatus::COUNT);

  name.append(".").append(TaskTypeToHistogramSuffix(task_type));
  base::UmaHistogramEnumeration(name, status, ScheduledTaskStatus::COUNT);
}

void LogsFileDirectoryCreationError(base::File::Error error) {
  // Maps to histogram enum PlatformFileError.
  UMA_HISTOGRAM_ENUMERATION("Download.Service.Files.DirCreationError", -error,
                            -base::File::Error::FILE_ERROR_MAX);
}

void LogFileCleanupStatus(FileCleanupReason reason,
                          int succeeded_cleanups,
                          int failed_cleanups,
                          int external_cleanups) {
  std::string name("Download.Service.Files.CleanUp.Success");
  base::UmaHistogramCounts100(name, succeeded_cleanups);
  name.append(".").append(FileCleanupReasonToHistogramSuffix(reason));
  base::UmaHistogramCounts100(name, succeeded_cleanups);

  name = "Download.Service.Files.CleanUp.Failure";
  base::UmaHistogramCounts100(name, failed_cleanups);
  name.append(".").append(FileCleanupReasonToHistogramSuffix(reason));
  base::UmaHistogramCounts100(name, failed_cleanups);

  name = "Download.Service.Files.CleanUp.External";
  base::UmaHistogramCounts100(name, external_cleanups);
  name.append(".").append(FileCleanupReasonToHistogramSuffix(reason));
  base::UmaHistogramCounts100(name, external_cleanups);
}

void LogFileLifeTime(const base::TimeDelta& file_life_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Download.Service.Files.LifeTime", file_life_time,
                             base::Seconds(1), base::Days(8), 100);
}

void LogEntryEvent(DownloadEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.Service.Entry.Event", event,
                            DownloadEvent::COUNT);
}

void LogEntryRetryCount(uint32_t retry_count) {
  UMA_HISTOGRAM_COUNTS_100("Download.Service.Entry.RetryCount", retry_count);
}

}  // namespace stats
}  // namespace download
