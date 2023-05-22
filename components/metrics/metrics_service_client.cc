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

const base::FeatureParam<int> kMaxLogQueueBytes{
    &features::kStructuredMetrics, "max_log_queue_bytes",
    300 * 1024  // 300 KiB
};

const base::FeatureParam<int> kMaxOngoingLogQueueCount{
    &features::kStructuredMetrics, "max_ongoing_log_queue_count", 8};

namespace {

// The minimum time in seconds between consecutive metrics report uploads.
constexpr int kMetricsUploadIntervalSecMinimum = 20;

// If a metrics log upload fails, and the transmission is over this byte count,
// then we will discard the log, and not try to retransmit it. We also don't
// persist the log to the prefs for transmission during the next chrome session
// if this limit is exceeded.
#if BUILDFLAG(IS_CHROMEOS)
// Increase CrOS limit to accommodate SampledProfile data (crbug.com/1210595).
constexpr size_t kMaxOngoingLogSize = 1024 * 1024;  // 1 MiB
#else
constexpr size_t kMaxOngoingLogSize = 100 * 1024;  // 100 KiB
#endif  // BUILDFLAG(IS_CHROMEOS)

// The minimum number of "initial" logs to save, and hope to send during a
// future Chrome session. Initial logs contain crash stats, and are pretty
// small.
constexpr size_t kMinInitialLogQueueCount = 20;

}  // namespace

MetricsServiceClient::MetricsServiceClient() {}

MetricsServiceClient::~MetricsServiceClient() {}

ukm::UkmService* MetricsServiceClient::GetUkmService() {
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

bool MetricsServiceClient::IsExternalExperimentAllowlistEnabled() {
  return true;
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
  // TODO(b/283126298): Rename min_* variable names to max_* to more accurately
  // reflect what the variable names represent.
  return {
      /*min_initial_log_queue_count=*/kMinInitialLogQueueCount,
      /*min_initial_log_queue_size=*/
      static_cast<size_t>(kMaxLogQueueBytes.Get()),
      /*min_ongoing_log_queue_count=*/
      static_cast<size_t>(kMaxOngoingLogQueueCount.Get()),
      /*min_ongoing_log_queue_size=*/
      static_cast<size_t>(kMaxLogQueueBytes.Get()),
      /*max_ongoing_log_size=*/kMaxOngoingLogSize,
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

absl::optional<bool> MetricsServiceClient::GetCurrentUserMetricsConsent()
    const {
  return absl::nullopt;
}

absl::optional<std::string> MetricsServiceClient::GetCurrentUserId() const {
  return absl::nullopt;
}

}  // namespace metrics
