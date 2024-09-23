// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_TEST_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_METRICS_TEST_TEST_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <set>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/test/test_metrics_log_uploader.h"

namespace variations {
class SyntheticTrialRegistry;
}

namespace metrics {

// A simple concrete implementation of the MetricsServiceClient interface, for
// use in tests.
class TestMetricsServiceClient : public MetricsServiceClient {
 public:
  static const char kBrandForTesting[];

  TestMetricsServiceClient();
  TestMetricsServiceClient(const TestMetricsServiceClient&) = delete;
  TestMetricsServiceClient& operator=(const TestMetricsServiceClient&) = delete;
  ~TestMetricsServiceClient() override;

  // MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  bool ShouldUploadMetricsForUserId(uint64_t user_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(base::OnceClosure done_callback) override;
  std::unique_ptr<MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      MetricsLogUploader::MetricServiceType service_type,
      const MetricsLogUploader::UploadCallback& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;
  bool IsReportingPolicyManaged() override;
  EnableMetricsDefault GetMetricsReportingDefaultState() override;
  std::string GetAppPackageNameIfLoggable() override;
  bool ShouldResetClientIdsOnClonedInstall() override;
  MetricsLogStore::StorageLimits GetStorageLimits() const override;

  // Adds/removes |user_id| from the set of user ids that have metrics consent
  // as true.
  void AllowMetricUploadForUserId(uint64_t user_id);
  void RemoveMetricUploadForUserId(uint64_t user_id);

  const std::string& get_client_id() const { return client_id_; }
  // Returns a weak ref to the last created uploader.
  TestMetricsLogUploader* uploader() { return uploader_.get(); }
  void set_version_string(const std::string& str) { version_string_ = str; }
  void set_product(int32_t product) { product_ = product; }
  void set_reporting_is_managed(bool managed) {
    reporting_is_managed_ = managed;
  }
  void set_is_extended_stable_channel(bool is_extended_stable_channel) {
    is_extended_stable_channel_ = is_extended_stable_channel;
  }
  void set_enable_default(EnableMetricsDefault enable_default) {
    enable_default_ = enable_default;
  }
  void set_should_reset_client_ids_on_cloned_install(bool state) {
    should_reset_client_ids_on_cloned_install_ = state;
  }
  void set_max_ongoing_log_size_bytes(size_t bytes) {
    storage_limits_.ongoing_log_queue_limits.max_log_size_bytes = bytes;
  }
  void set_min_ongoing_log_queue_count(size_t log_count) {
    storage_limits_.ongoing_log_queue_limits.min_log_count = log_count;
  }
  void set_min_ongoing_log_queue_size_bytes(size_t bytes) {
    storage_limits_.ongoing_log_queue_limits.min_queue_size_bytes = bytes;
  }
  void set_synthetic_trial_registry(
      variations::SyntheticTrialRegistry* registry) {
    synthetic_trial_registry_ = registry;
  }

 private:
  std::string client_id_{"0a94430b-18e5-43c8-a657-580f7e855ce1"};
  std::string version_string_{"5.0.322.0-64-devel"};
  int32_t product_ = ChromeUserMetricsExtension::CHROME;
  bool reporting_is_managed_ = false;
  bool is_extended_stable_channel_ = false;
  EnableMetricsDefault enable_default_ = EnableMetricsDefault::DEFAULT_UNKNOWN;
  bool should_reset_client_ids_on_cloned_install_ = false;
  MetricsLogStore::StorageLimits storage_limits_ =
      MetricsServiceClient::GetStorageLimits();
  std::set<uint64_t> allowed_user_ids_;

  raw_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_ =
      nullptr;

  // A weak ref to the last created TestMetricsLogUploader.
  base::WeakPtr<TestMetricsLogUploader> uploader_ = nullptr;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_TEST_METRICS_SERVICE_CLIENT_H_
