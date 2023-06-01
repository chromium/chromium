// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_metrics_service_client.h"

#include "base/test/task_environment.h"
#include "chromecast/metrics/mock_cast_sys_info_util.h"
#include "components/metrics/metrics_log_store.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace metrics {
namespace {

class FakeCastMetricsServiceDelegate : public CastMetricsServiceDelegate {
 public:
  void SetStorageLimits(::metrics::MetricsLogStore::StorageLimits limits) {
    storage_limits_ = std::move(limits);
  }

 private:
  void SetMetricsClientId(const std::string& client_id) override {}
  void RegisterMetricsProviders(::metrics::MetricsService* service) override {}
  void ApplyMetricsStorageLimits(
      ::metrics::MetricsLogStore::StorageLimits* limits) override {
    *limits = std::move(storage_limits_);
  }

  ::metrics::MetricsLogStore::StorageLimits storage_limits_;
};

class CastMetricsServiceClientTest : public testing::Test {
 public:
  CastMetricsServiceClientTest() {}
  CastMetricsServiceClientTest(const CastMetricsServiceClientTest&) = delete;
  CastMetricsServiceClientTest& operator=(const CastMetricsServiceClientTest&) =
      delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

/**
 * Validates that class instatiation and calls to GetChannel() only
 * result in a single method invocation of CreateSysInfo().
 */
TEST_F(CastMetricsServiceClientTest, CreateSysInfoSingleInvocation) {
  EXPECT_EQ(chromecast::GetSysInfoCreatedCount(), 0);

  CastMetricsServiceClient client(nullptr, nullptr, nullptr);

  client.GetChannel();
  client.GetChannel();

  // Despite muiltiple calls to GetChannel(),
  // SysInfo should only be created a single time
  EXPECT_EQ(chromecast::GetSysInfoCreatedCount(), 1);
}

TEST_F(CastMetricsServiceClientTest, UsesDelegateToGetStorageLimits) {
  FakeCastMetricsServiceDelegate delegate;
  CastMetricsServiceClient client(&delegate, nullptr, nullptr);

  // Set arbitrary limits to ensure the limits propagate to the client
  // correctly.
  ::metrics::MetricsLogStore::StorageLimits expected_limits = {
      .initial_log_queue_limits =
          ::metrics::UnsentLogStore::UnsentLogStoreLimits{
              .min_log_count = 10,
              .min_queue_size_bytes = 2000,
          },
      .ongoing_log_queue_limits =
          ::metrics::UnsentLogStore::UnsentLogStoreLimits{
              .min_log_count = 30,
              .min_queue_size_bytes = 4000,
              .max_log_size_bytes = 5000,
          },
  };
  delegate.SetStorageLimits(expected_limits);
  ::metrics::MetricsLogStore::StorageLimits actual_limits =
      client.GetStorageLimits();
  EXPECT_EQ(actual_limits.initial_log_queue_limits.min_log_count,
            expected_limits.initial_log_queue_limits.min_log_count);
  EXPECT_EQ(actual_limits.initial_log_queue_limits.min_queue_size_bytes,
            expected_limits.initial_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(actual_limits.initial_log_queue_limits.max_log_size_bytes,
            expected_limits.initial_log_queue_limits.max_log_size_bytes);
  EXPECT_EQ(actual_limits.ongoing_log_queue_limits.min_log_count,
            expected_limits.ongoing_log_queue_limits.min_log_count);
  EXPECT_EQ(actual_limits.ongoing_log_queue_limits.min_queue_size_bytes,
            expected_limits.ongoing_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(actual_limits.ongoing_log_queue_limits.max_log_size_bytes,
            expected_limits.ongoing_log_queue_limits.max_log_size_bytes);
}

}  // namespace
}  // namespace metrics
}  // namespace chromecast
