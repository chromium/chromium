// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test/test_metrics_service_client.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "components/metrics/metrics_log_uploader.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

// static
const char TestMetricsServiceClient::kBrandForTesting[] = "brand_for_testing";

TestMetricsServiceClient::TestMetricsServiceClient() = default;
TestMetricsServiceClient::~TestMetricsServiceClient() = default;

variations::SyntheticTrialRegistry*
TestMetricsServiceClient::GetSyntheticTrialRegistry() {
  return synthetic_trial_registry_;
}

metrics::MetricsService* TestMetricsServiceClient::GetMetricsService() {
  return nullptr;
}

void TestMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  client_id_ = client_id;
}

bool TestMetricsServiceClient::ShouldUploadMetricsForUserId(uint64_t user_id) {
  return base::Contains(allowed_user_ids_, user_id);
}

int32_t TestMetricsServiceClient::GetProduct() {
  return product_;
}

std::string TestMetricsServiceClient::GetApplicationLocale() {
  return "en-US";
}

bool TestMetricsServiceClient::GetBrand(std::string* brand_code) {
  *brand_code = kBrandForTesting;
  return true;
}

const network_time::NetworkTimeTracker*
TestMetricsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

SystemProfileProto::Channel TestMetricsServiceClient::GetChannel() {
  return SystemProfileProto::CHANNEL_BETA;
}

bool TestMetricsServiceClient::IsExtendedStableChannel() {
  return is_extended_stable_channel_;
}

std::string TestMetricsServiceClient::GetVersionString() {
  return version_string_;
}

void TestMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  std::move(done_callback).Run();
}

std::unique_ptr<MetricsLogUploader> TestMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    std::string_view mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete) {
  auto uploader = std::make_unique<TestMetricsLogUploader>(on_upload_complete);
  uploader_ = uploader->AsWeakPtr();
  return uploader;
}

base::TimeDelta TestMetricsServiceClient::GetStandardUploadInterval() {
  return base::Minutes(5);
}

bool TestMetricsServiceClient::IsReportingPolicyManaged() {
  return reporting_is_managed_;
}

EnableMetricsDefault
TestMetricsServiceClient::GetMetricsReportingDefaultState() {
  return enable_default_;
}

std::string TestMetricsServiceClient::GetAppPackageNameIfLoggable() {
  return "test app";
}

bool TestMetricsServiceClient::ShouldResetClientIdsOnClonedInstall() {
  return should_reset_client_ids_on_cloned_install_;
}

MetricsLogStore::StorageLimits TestMetricsServiceClient::GetStorageLimits()
    const {
  return storage_limits_;
}

void TestMetricsServiceClient::AllowMetricUploadForUserId(uint64_t user_id) {
  allowed_user_ids_.insert(user_id);
}

void TestMetricsServiceClient::RemoveMetricUploadForUserId(uint64_t user_id) {
  allowed_user_ids_.erase(user_id);
}

}  // namespace metrics
