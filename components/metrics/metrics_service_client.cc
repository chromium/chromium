// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_client.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/url_constants.h"
#include "metrics_service_client.h"

namespace metrics {

namespace {

// The number of initial/ongoing logs to persist in the queue before logs are
// dropped.
// Note: Both the count threshold and the bytes threshold (see
// `kLogBytesTrimThreshold` below) must be reached for logs to be
// dropped/trimmed.
//
// Note that each ongoing log may be pretty large, since "initial" logs must
// first be sent before any ongoing logs are transmitted. "Initial" logs will
// not be sent if a user is offline. As a result, the current ongoing log will
// accumulate until the "initial" log can be transmitted. We don't want to save
// too many of these mega-logs (this should be capped by
// kLogBytesTrimThreshold).
//
// A "standard shutdown" will create a small log, including just the data that
// was not yet been transmitted, and that is normal (to have exactly one
// ongoing log at startup).
//
// Refer to //components/metrics/unsent_log_store.h for more details on when
// logs are dropped.
const base::FeatureParam<int> kInitialLogCountTrimThreshold{
    &features::kMetricsLogTrimming, "initial_log_count_trim_threshold", 20};
const base::FeatureParam<int> kOngoingLogCountTrimThreshold{
    &features::kMetricsLogTrimming, "ongoing_log_count_trim_threshold", 8};

// The number bytes of the queue to be persisted before logs are dropped. This
// will be applied to both log queues (initial/ongoing). This ensures that a
// reasonable amount of history will be stored even if there is a long series of
// very small logs.
// Note: Both the count threshold (see `kInitialLogCountTrimThreshold` and
// `kOngoingLogCountTrimThreshold` above) and the bytes threshold must be
// reached for logs to be dropped/trimmed.
//
// Refer to //components/metrics/unsent_log_store.h for more details on when
// logs are dropped.
const base::FeatureParam<int> kLogBytesTrimThreshold{
    &features::kMetricsLogTrimming, "log_bytes_trim_threshold",
    300 * 1024  // 300 KiB
};

// If an initial/ongoing metrics log upload fails, and the transmission is over
// this byte count, then we will discard the log, and not try to retransmit it.
// We also don't persist the log to the prefs for transmission during the next
// chrome session if this limit is exceeded.
const base::FeatureParam<int> kMaxInitialLogSizeBytes{
    &features::kMetricsLogTrimming, "max_initial_log_size_bytes",
    0  // Initial logs can be of any size.
};
const base::FeatureParam<int> kMaxOngoingLogSizeBytes{
    &features::kMetricsLogTrimming, "max_ongoing_log_size_bytes",
#if BUILDFLAG(IS_CHROMEOS)
    // Increase CrOS limit to accommodate SampledProfile data (crbug/1210595).
    1024 * 1024  // 1 MiB
#else
    100 * 1024  // 100 KiB
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// The minimum time in seconds between consecutive metrics report uploads.
constexpr int kMetricsUploadIntervalSecMinimum = 20;

}  // namespace

MetricsServiceClient::MetricsServiceClient() = default;

MetricsServiceClient::~MetricsServiceClient() = default;

ukm::UkmService* MetricsServiceClient::GetUkmService() {
  return nullptr;
}

IdentifiabilityStudyState*
MetricsServiceClient::GetIdentifiabilityStudyState() {
  return nullptr;
}

structured::StructuredMetricsService*
MetricsServiceClient::GetStructuredMetricsService() {
  return nullptr;
}

bool MetricsServiceClient::ShouldUploadMetricsForUserId(uint64_t user_id) {
  return true;
}

GURL MetricsServiceClient::GetMetricsServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUmaServerUrl)) {
    return GURL(command_line->GetSwitchValueASCII(switches::kUmaServerUrl));
  }
  return GURL(kNewMetricsServerUrl);
}

GURL MetricsServiceClient::GetInsecureMetricsServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUmaInsecureServerUrl)) {
    return GURL(
        command_line->GetSwitchValueASCII(switches::kUmaInsecureServerUrl));
  }
  return GURL(kNewMetricsServerUrlInsecure);
}

base::TimeDelta MetricsServiceClient::GetUploadInterval() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // If an upload interval is set from the command line, use that value but
  // subject it to a minimum threshold to mitigate the risk of DDoS attack.
  if (command_line->HasSwitch(metrics::switches::kMetricsUploadIntervalSec)) {
    const std::string switch_value = command_line->GetSwitchValueASCII(
        metrics::switches::kMetricsUploadIntervalSec);
    int custom_upload_interval;
    if (base::StringToInt(switch_value, &custom_upload_interval)) {
      return base::Seconds(
          std::max(custom_upload_interval, kMetricsUploadIntervalSecMinimum));
    }
    LOG(DFATAL) << "Malformed value for --metrics-upload-interval. "
                << "Expected int, got: " << switch_value;
  }
  return GetStandardUploadInterval();
}

bool MetricsServiceClient::ShouldStartUpFastForTesting() const {
  return false;
}

bool MetricsServiceClient::IsReportingPolicyManaged() {
  return false;
}

EnableMetricsDefault MetricsServiceClient::GetMetricsReportingDefaultState() {
  return EnableMetricsDefault::DEFAULT_UNKNOWN;
}

bool MetricsServiceClient::IsOnCellularConnection() {
  return false;
}

bool MetricsServiceClient::IsUkmAllowedForAllProfiles() {
  return false;
}

bool MetricsServiceClient::AreNotificationListenersEnabledOnAllProfiles() {
  return false;
}

std::string MetricsServiceClient::GetAppPackageNameIfLoggable() {
  return std::string();
}

std::string MetricsServiceClient::GetUploadSigningKey() {
  return std::string();
}

bool MetricsServiceClient::ShouldResetClientIdsOnClonedInstall() {
  return false;
}

base::CallbackListSubscription
MetricsServiceClient::AddOnClonedInstallDetectedCallback(
    base::OnceClosure callback) {
  return base::CallbackListSubscription();
}

MetricsLogStore::StorageLimits MetricsServiceClient::GetStorageLimits() const {
  return {
      .initial_log_queue_limits =
          UnsentLogStore::UnsentLogStoreLimits{
              .min_log_count =
                  static_cast<size_t>(kInitialLogCountTrimThreshold.Get()),
              .min_queue_size_bytes =
                  static_cast<size_t>(kLogBytesTrimThreshold.Get()),
              .max_log_size_bytes =
                  static_cast<size_t>(kMaxInitialLogSizeBytes.Get()),
          },
      .ongoing_log_queue_limits =
          UnsentLogStore::UnsentLogStoreLimits{
              .min_log_count =
                  static_cast<size_t>(kOngoingLogCountTrimThreshold.Get()),
              .min_queue_size_bytes =
                  static_cast<size_t>(kLogBytesTrimThreshold.Get()),
              .max_log_size_bytes =
                  static_cast<size_t>(kMaxOngoingLogSizeBytes.Get()),
          },
  };
}

void MetricsServiceClient::SetUpdateRunningServicesCallback(
    const base::RepeatingClosure& callback) {
  update_running_services_ = callback;
}

void MetricsServiceClient::UpdateRunningServices() {
  if (update_running_services_) {
    update_running_services_.Run();
  }
}

bool MetricsServiceClient::IsMetricsReportingForceEnabled() const {
  return ::metrics::IsMetricsReportingForceEnabled();
}

std::optional<bool> MetricsServiceClient::GetCurrentUserMetricsConsent() const {
  return std::nullopt;
}

std::optional<std::string> MetricsServiceClient::GetCurrentUserId() const {
  return std::nullopt;
}

}  // namespace metrics
