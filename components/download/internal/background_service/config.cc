// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/config.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/download/public/background_service/features.h"

namespace download {

namespace {

// Default value for battery query interval.
const base::TimeDelta kDefaultBatteryQueryInterval = base::Seconds(120);

// Default minimum battery percentage to start download or background task when
// battery requirement is sensitive.
const uint32_t kDefaultDownloadBatteryPercentage = 50;

// Default value for max concurrent downloads configuration.
const uint32_t kDefaultMaxConcurrentDownloads = 4;

// Default value for maximum running downloads of the download service.
const uint32_t kDefaultMaxRunningDownloads = 2;

// Default value for maximum scheduled downloads.
const uint32_t kDefaultMaxScheduledDownloads = 15;

// Default value for maximum retry count.
const uint32_t kDefaultMaxRetryCount = 5;

// Default value for maximum resumption count.
const uint32_t kDefaultMaxResumptionCount = 15;

// Default value for file keep alive time, keep the file alive for 12 hours by
// default.
const base::TimeDelta kDefaultFileKeepAliveTime = base::Hours(12);

// Default value for maximum duration that the file can be kept alive time,
// default is 7 days.
const base::TimeDelta kDefaultMaxFileKeepAliveTime = base::Days(7);

// Default value for file cleanup window, the system will schedule a cleanup
// task within this window.
const base::TimeDelta kDefaultFileCleanupWindow = base::Hours(24);

// Default value for the start window time for OS to schedule background task.
const base::TimeDelta kDefaultWindowStartTime = base::Minutes(5);

// Default value for the end window time for OS to schedule background task.
const base::TimeDelta kDefaultWindowEndTime = base::Hours(8);

// Default value for start up delay to wait for network stack ready.
const base::TimeDelta kDefaultNetworkStartupDelay = base::Seconds(2);

// Default value for start up delay to wait for network stack ready when
// triggered from a background task.
const base::TimeDelta kDefaultNetworkStartupDelayBackgroundTask =
    base::Seconds(5);

// The default delay to notify the observer when network changes from
// disconnected to connected.
const base::TimeDelta kDefaultNetworkChangeDelay = base::Seconds(5);

// The default delay to notify the observer after a navigation completes.
const base::TimeDelta kDefaultNavigationCompletionDelay = base::Seconds(30);

// The default timeout for a navigation.
const base::TimeDelta kDefaultNavigationTimeoutDelay = base::Seconds(300);

// The default timeout for a pending upload.
const base::TimeDelta kDefaultPendingUploadTimeoutDelay = base::Seconds(30);

// The default value of download retry delay when the download is failed.
const base::TimeDelta kDefaultDownloadRetryDelay = base::Seconds(20);

// Helper routine to get Finch experiment parameter. If no Finch seed was found,
// use the |default_value|. The |name| should match an experiment
// parameter in Finch server configuration.
uint32_t GetFinchConfigUInt(const std::string& name, uint32_t default_value) {
  std::string finch_value =
      base::GetFieldTrialParamValueByFeature(kDownloadServiceFeature, name);
  uint32_t result;
  return base::StringToUint(finch_value, &result) ? result : default_value;
}

}  // namespace

// static
std::unique_ptr<Configuration> Configuration::CreateFromFinch() {
  std::unique_ptr<Configuration> config(new Configuration());
  config->battery_query_interval = base::Seconds(base::saturated_cast<int>(
      GetFinchConfigUInt(kBatteryQueryIntervalConfig,
                         kDefaultBatteryQueryInterval.InSeconds())));
  config->download_battery_percentage =
      base::saturated_cast<int>(GetFinchConfigUInt(
          kDownloadBatteryPercentageConfig, kDefaultDownloadBatteryPercentage));
  config->max_concurrent_downloads = GetFinchConfigUInt(
      kMaxConcurrentDownloadsConfig, kDefaultMaxConcurrentDownloads);
  config->max_running_downloads = GetFinchConfigUInt(
      kMaxRunningDownloadsConfig, kDefaultMaxRunningDownloads);
  config->max_scheduled_downloads = GetFinchConfigUInt(
      kMaxScheduledDownloadsConfig, kDefaultMaxScheduledDownloads);
  config->max_retry_count =
      GetFinchConfigUInt(kMaxRetryCountConfig, kDefaultMaxRetryCount);
  config->max_resumption_count =
      GetFinchConfigUInt(kMaxResumptionCountConfig, kDefaultMaxResumptionCount);
  config->file_keep_alive_time = base::Minutes(base::saturated_cast<int>(
      GetFinchConfigUInt(kFileKeepAliveTimeMinutesConfig,
                         kDefaultFileKeepAliveTime.InMinutes())));
  config->max_file_keep_alive_time = base::Minutes(base::saturated_cast<int>(
      GetFinchConfigUInt(kMaxFileKeepAliveTimeMinutesConfig,
                         kDefaultMaxFileKeepAliveTime.InMinutes())));
  config->file_cleanup_window = base::Minutes(base::saturated_cast<int>(
      GetFinchConfigUInt(kFileCleanupWindowMinutesConfig,
                         kDefaultFileCleanupWindow.InMinutes())));
  config->window_start_time =
      base::Seconds(base::saturated_cast<int>(GetFinchConfigUInt(
          kWindowStartTimeSecondsConfig, kDefaultWindowStartTime.InSeconds())));
  config->window_end_time =
      base::Seconds(base::saturated_cast<int>(GetFinchConfigUInt(
          kWindowEndTimeSecondsConfig, kDefaultWindowEndTime.InSeconds())));
  config->network_startup_delay = base::Milliseconds(base::saturated_cast<int>(
      GetFinchConfigUInt(kNetworkStartupDelayMsConfig,
                         kDefaultNetworkStartupDelay.InMilliseconds())));
  config->network_startup_delay_backgroud_task =
      base::Milliseconds(base::saturated_cast<int>(GetFinchConfigUInt(
          kNetworkStartupDelayBackgroundTaskMsConfig,
          kDefaultNetworkStartupDelayBackgroundTask.InMilliseconds())));

  config->network_change_delay = base::Milliseconds(base::saturated_cast<int>(
      GetFinchConfigUInt(kNetworkChangeDelayMsConfig,
                         kDefaultNetworkChangeDelay.InMilliseconds())));
  config->navigation_completion_delay = base::Seconds(base::saturated_cast<int>(
      GetFinchConfigUInt(kNavigationCompletionDelaySecondsConfig,
                         kDefaultNavigationCompletionDelay.InSeconds())));
  config->navigation_timeout_delay = base::Seconds(base::saturated_cast<int>(
      GetFinchConfigUInt(kNavigationTimeoutDelaySecondsConfig,
                         kDefaultNavigationTimeoutDelay.InSeconds())));
  config->pending_upload_timeout_delay =
      base::Seconds(base::saturated_cast<int>(
          GetFinchConfigUInt(kPendingUploadTimeoutDelaySecondsConfig,
                             kDefaultPendingUploadTimeoutDelay.InSeconds())));

  config->download_retry_delay = base::Milliseconds(base::saturated_cast<int>(
      GetFinchConfigUInt(kDownloadRetryDelayMsConfig,
                         kDefaultDownloadRetryDelay.InMilliseconds())));
  return config;
}

Configuration::Configuration()
    : battery_query_interval(kDefaultBatteryQueryInterval),
      download_battery_percentage(kDefaultDownloadBatteryPercentage),
      max_concurrent_downloads(kDefaultMaxConcurrentDownloads),
      max_running_downloads(kDefaultMaxRunningDownloads),
      max_scheduled_downloads(kDefaultMaxScheduledDownloads),
      max_retry_count(kDefaultMaxRetryCount),
      max_resumption_count(kDefaultMaxResumptionCount),
      file_keep_alive_time(kDefaultFileKeepAliveTime),
      max_file_keep_alive_time(kDefaultMaxFileKeepAliveTime),
      file_cleanup_window(kDefaultFileCleanupWindow),
      window_start_time(kDefaultWindowStartTime),
      window_end_time(kDefaultWindowEndTime),
      network_startup_delay(kDefaultNetworkStartupDelay),
      network_change_delay(kDefaultNetworkChangeDelay),
      navigation_completion_delay(kDefaultNavigationCompletionDelay),
      navigation_timeout_delay(kDefaultNavigationTimeoutDelay),
      pending_upload_timeout_delay(kDefaultPendingUploadTimeoutDelay),
      download_retry_delay(kDefaultDownloadRetryDelay) {}

}  // namespace download
