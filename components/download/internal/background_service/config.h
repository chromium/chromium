// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONFIG_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONFIG_H_

#include <memory>

#include "base/time/time.h"

namespace download {

// Configuration name for max concurrent downloads.
constexpr char kBatteryQueryIntervalConfig[] = "battery_query_interval_seconds";

// Configuration name for download battery percentage.
constexpr char kDownloadBatteryPercentageConfig[] =
    "download_battery_percentage";

// Configuration name for max concurrent downloads.
constexpr char kMaxConcurrentDownloadsConfig[] = "max_concurrent_downloads";

// Configuration name for maximum running downloads.
constexpr char kMaxRunningDownloadsConfig[] = "max_running_downloads";

// Configuration name for maximum scheduled downloads.
constexpr char kMaxScheduledDownloadsConfig[] = "max_scheduled_downloads";

// Configuration name for maximum retry count.
constexpr char kMaxRetryCountConfig[] = "max_retry_count";

// Configuration name for maximum resumption count.
constexpr char kMaxResumptionCountConfig[] = "max_resumption_count";

// Configuration name for file keep alive time.
constexpr char kFileKeepAliveTimeMinutesConfig[] =
    "file_keep_alive_time_minutes";

// Configuration name for maximum duration that the file can be kept alive.
constexpr char kMaxFileKeepAliveTimeMinutesConfig[] =
    "max_file_keep_alive_time_minutes";

// Configuration name for file keep alive time.
constexpr char kFileCleanupWindowMinutesConfig[] = "file_cleanup_window";

// Configuration name for window start time.
constexpr char kWindowStartTimeSecondsConfig[] = "window_start_time_seconds";

// Configuration name for window end time.
constexpr char kWindowEndTimeSecondsConfig[] = "window_end_time_seconds";

// Configuration name for start up delay, measured in milliseconds.
constexpr char kNetworkStartupDelayMsConfig[] = "start_up_delay_ms";

// Configuration name for start up delay when triggered from a background task,
// measured in milliseconds.
constexpr char kNetworkStartupDelayBackgroundTaskMsConfig[] =
    "start_up_delay_background_task_ms";

// Configuration name for the delay to notify network status change, measured in
// milliseconds.
constexpr char kNetworkChangeDelayMsConfig[] = "network_change_delay_ms";

// Configuration name for the download resumption delay after a navigation
// completes, measured in seconds.
constexpr char kNavigationCompletionDelaySecondsConfig[] =
    "navigation_completion_delay_seconds";

// Configuration name for the timeout value from the start of a navigation after
// which if still no response, download service will resume. Measured in
// seconds.
constexpr char kNavigationTimeoutDelaySecondsConfig[] =
    "navigation_timeout_delay_seconds";

// Configuration name for the minimum timeout value after which an upload can be
// killed if the client still hasn't responded with the upload data. Measured in
// seconds.
constexpr char kPendingUploadTimeoutDelaySecondsConfig[] =
    "pending_upload_timeout_delay_seconds";

// Configuration name for the retry delay when the download is failed, measured
// in milliseconds.
constexpr char kDownloadRetryDelayMsConfig[] = "retry_delay_ms";

// Download service configuration.
//
// Loaded based on experiment parameters from the server. Use default values if
// no server configuration was detected.
struct Configuration {
 public:
  // Create the configuration.
  static std::unique_ptr<Configuration> CreateFromFinch();
  Configuration();

  Configuration(const Configuration&) = delete;
  Configuration& operator=(const Configuration&) = delete;

  // An interval to throttle battery status queries.
  base::TimeDelta battery_query_interval;

  // Minimum battery percentage to start the background task or start the
  // download when battery requirement is sensitive.
  int download_battery_percentage;

  // The maximum number of downloads the DownloadService can have currently in
  // Active or Paused states.
  uint32_t max_concurrent_downloads;

  // The maximum number of downloads the DownloadService can have currently in
  // only Active state.
  uint32_t max_running_downloads;

  // The maximum number of downloads that are scheduled for each client using
  // the download service.
  uint32_t max_scheduled_downloads;

  // The maximum number of retries before the download is aborted.
  uint32_t max_retry_count;

  // The maximum number of conceptually 'free' resumptions before the download
  // is aborted.  This is a failsafe to prevent constantly hammering the source.
  uint32_t max_resumption_count;

  // The time that the download service will keep the files around before
  // deleting them if the client hasn't handle the files.
  base::TimeDelta file_keep_alive_time;

  // The maximum time that the download service can keep the files around before
  // forcefully deleting them even if the client doesn't agree.
  base::TimeDelta max_file_keep_alive_time;

  // The length of the flexible time window during which the scheduler must
  // schedule a file cleanup task.
  base::TimeDelta file_cleanup_window;

  // The start window time in seconds for OS to schedule background task.
  // The OS will trigger the background task in this window.
  base::TimeDelta window_start_time;

  // The end window time in seconds for OS to schedule background task.
  // The OS will trigger the background task in this window.
  base::TimeDelta window_end_time;

  // The delay to initialize internal components to wait for network stack
  // ready.
  base::TimeDelta network_startup_delay;

  // The delay to initialize internal components to wait for network stack
  // ready when triggered from a background task.
  base::TimeDelta network_startup_delay_backgroud_task;

  // The delay to notify network status changes.
  base::TimeDelta network_change_delay;

  // The delay to notify about the navigation completion.
  base::TimeDelta navigation_completion_delay;

  // The timeout to wait for after a navigation starts.
  base::TimeDelta navigation_timeout_delay;

  // The minimum timeout after which upload entries waiting on data from their
  // clients might be killed.
  base::TimeDelta pending_upload_timeout_delay;

  // The delay to retry a download when the download is failed.
  base::TimeDelta download_retry_delay;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONFIG_H_
