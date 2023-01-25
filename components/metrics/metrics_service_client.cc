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
#include "components/metrics/metrics_switches.h"
#include "components/metrics/url_constants.h"

namespace metrics {
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

// The number of bytes of logs to save of each type (initial/ongoing). This
// ensures that a reasonable amount of history will be stored even if there is a
// long series of very small logs.
constexpr size_t kMinLogQueueSize = 300 * 1024;  // 300 KiB

// The minimum number of "initial" logs to save, and hope to send during a
// future Chrome session. Initial logs contain crash stats, and are pretty
// small.
constexpr size_t kMinInitialLogQueueCount = 20;

// The minimum number of ongoing logs to save persistently, and hope to send
// during a this or future sessions. Note that each log may be pretty large, as
// presumably the related "initial" log wasn't sent (probably nothing was, as
// the user was probably off-line). As a result, the log probably kept
// accumulating while the "initial" log was stalled, and couldn't be sent. As a
// result, we don't want to save too many of these mega-logs. A "standard
// shutdown" will create a small log, including just the data that was not yet
// been transmitted, and that is normal (to have exactly one ongoing_log_ at
// startup).
constexpr size_t kMinOngoingLogQueueCount = 8;

}  // namespace

MetricsServiceClient::MetricsServiceClient() {}

MetricsServiceClient::~MetricsServiceClient() {}

ukm::UkmService* MetricsServiceClient::GetUkmService() {
  return nullptr;
}

bool MetricsServiceClient::ShouldUploadMetricsForUserId(uint64_t user_id) {
  return true;
}

GURL MetricsServiceClient::GetMetricsServerUrl() {
#ifndef NDEBUG
  // Only allow overriding the server URL through the command line in debug
  // builds. This is to prevent, for example, rerouting metrics due to malware.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUmaServerUrl))
    return GURL(command_line->GetSwitchValueASCII(switches::kUmaServerUrl));
#endif  // NDEBUG
  return GURL(kNewMetricsServerUrl);
}

GURL MetricsServiceClient::GetInsecureMetricsServerUrl() {
#ifndef NDEBUG
  // Only allow overriding the server URL through the command line in debug
  // builds. This is to prevent, for example, rerouting metrics due to malware.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUmaInsecureServerUrl)) {
    return GURL(
        command_line->GetSwitchValueASCII(switches::kUmaInsecureServerUrl));
  }
#endif  // NDEBUG
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

bool MetricsServiceClient::IsUMACellularUploadLogicEnabled() {
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
  return {
      /*min_initial_log_queue_count=*/kMinInitialLogQueueCount,
      /*min_initial_log_queue_size=*/kMinLogQueueSize,
      /*min_ongoing_log_queue_count=*/kMinOngoingLogQueueCount,
      /*min_ongoing_log_queue_size=*/kMinLogQueueSize,
      /*max_ongoing_log_size=*/kMaxOngoingLogSize,
  };
}

void MetricsServiceClient::SetUpdateRunningServicesCallback(
    const base::RepeatingClosure& callback) {
  update_running_services_ = callback;
}

void MetricsServiceClient::UpdateRunningServices() {
  if (update_running_services_)
    update_running_services_.Run();
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
