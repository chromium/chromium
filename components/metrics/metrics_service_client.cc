// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_client.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/url_constants.h"

namespace metrics {

namespace {

// The minimum time in seconds between consecutive metrics report uploads.
constexpr int kMetricsUploadIntervalSecMinimum = 20;

}  // namespace

MetricsServiceClient::MetricsServiceClient() {}

MetricsServiceClient::~MetricsServiceClient() {}

ukm::UkmService* MetricsServiceClient::GetUkmService() {
  return nullptr;
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

GURL MetricsServiceClient::GetMetricsServerUrl() {
  return GURL(kNewMetricsServerUrl);
}

GURL MetricsServiceClient::GetInsecureMetricsServerUrl() {
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
      return base::TimeDelta::FromSeconds(
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

bool MetricsServiceClient::IsUkmAllowedForAllProfiles() {
  return false;
}

bool MetricsServiceClient::IsUkmAllowedWithExtensionsForAllProfiles() {
  return false;
}

bool MetricsServiceClient::AreNotificationListenersEnabledOnAllProfiles() {
  return false;
}

std::string MetricsServiceClient::GetAppPackageName() {
  return std::string();
}

std::string MetricsServiceClient::GetUploadSigningKey() {
  return std::string();
}

void MetricsServiceClient::SetUpdateRunningServicesCallback(
    const base::Closure& callback) {
  update_running_services_ = callback;
}

void MetricsServiceClient::UpdateRunningServices() {
  if (update_running_services_)
    update_running_services_.Run();
}

bool MetricsServiceClient::IsMetricsReportingForceEnabled() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceEnableMetricsReporting);
}

}  // namespace metrics
