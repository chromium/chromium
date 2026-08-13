// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

class MetricsServiceClientTest : public testing::Test {
 public:
  MetricsServiceClientTest() = default;

  MetricsServiceClientTest(const MetricsServiceClientTest&) = delete;
  MetricsServiceClientTest& operator=(const MetricsServiceClientTest&) = delete;

  ~MetricsServiceClientTest() override = default;
};

}  // namespace

TEST_F(MetricsServiceClientTest, TestUploadIntervalDefaultsToStandard) {
  TestMetricsServiceClient client;

  ASSERT_EQ(client.GetStandardUploadInterval(), client.GetUploadInterval());
}

TEST_F(MetricsServiceClientTest, TestModifyMetricsUploadInterval) {
  TestMetricsServiceClient client;

  // Flip it a few times to make sure we really can modify it. Values are
  // arbitrary (but positive, because the upload interval should be).
  int specified_upload_sec = 800;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMetricsUploadIntervalSec,
      base::NumberToString(specified_upload_sec));
  ASSERT_EQ(base::Seconds(specified_upload_sec), client.GetUploadInterval());

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kMetricsUploadIntervalSec);

  specified_upload_sec = 30;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMetricsUploadIntervalSec,
      base::NumberToString(specified_upload_sec));
  ASSERT_EQ(base::Seconds(specified_upload_sec), client.GetUploadInterval());
}

TEST_F(MetricsServiceClientTest, TestUploadIntervalLimitedForDos) {
  TestMetricsServiceClient client;

  // If we set the upload interval too small, it should be limited to prevent
  // the possibility of DOS'ing the backend. This should be a safe guess for a
  // value strictly smaller than the DOS limit.
  int too_short_upload_sec = 2;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMetricsUploadIntervalSec,
      base::NumberToString(too_short_upload_sec));
  // Upload interval should be the DOS rate limit.
  ASSERT_EQ(base::Seconds(20), client.GetUploadInterval());
}

TEST_F(MetricsServiceClientTest, TestGetStorageLimits) {
  TestMetricsServiceClient client;
  const MetricsLogStore::StorageLimits storage_limits =
      client.GetStorageLimits();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_EQ(20u, storage_limits.initial_log_queue_limits.min_log_count);
  EXPECT_EQ(8u, storage_limits.ongoing_log_queue_limits.min_log_count);
  EXPECT_EQ(3u * 1024 * 1024,
            storage_limits.initial_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(3u * 1024 * 1024,
            storage_limits.ongoing_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(0u, storage_limits.initial_log_queue_limits.max_log_size_bytes);
  EXPECT_EQ(1024u * 1024,
            storage_limits.ongoing_log_queue_limits.max_log_size_bytes);
#elif BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(20u, storage_limits.initial_log_queue_limits.min_log_count);
  EXPECT_EQ(8u, storage_limits.ongoing_log_queue_limits.min_log_count);
  EXPECT_EQ(300u * 1024,
            storage_limits.initial_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(300u * 1024,
            storage_limits.ongoing_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(0u, storage_limits.initial_log_queue_limits.max_log_size_bytes);
  EXPECT_EQ(1024u * 1024,
            storage_limits.ongoing_log_queue_limits.max_log_size_bytes);
#elif BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(40u, storage_limits.initial_log_queue_limits.min_log_count);
  EXPECT_EQ(16u, storage_limits.ongoing_log_queue_limits.min_log_count);
  EXPECT_EQ(600u * 1024,
            storage_limits.initial_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(600u * 1024,
            storage_limits.ongoing_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(0u, storage_limits.initial_log_queue_limits.max_log_size_bytes);
  EXPECT_EQ(200u * 1024,
            storage_limits.ongoing_log_queue_limits.max_log_size_bytes);
#else
  EXPECT_EQ(20u, storage_limits.initial_log_queue_limits.min_log_count);
  EXPECT_EQ(8u, storage_limits.ongoing_log_queue_limits.min_log_count);
  EXPECT_EQ(300u * 1024,
            storage_limits.initial_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(300u * 1024,
            storage_limits.ongoing_log_queue_limits.min_queue_size_bytes);
  EXPECT_EQ(0u, storage_limits.initial_log_queue_limits.max_log_size_bytes);
  EXPECT_EQ(100u * 1024,
            storage_limits.ongoing_log_queue_limits.max_log_size_bytes);
#endif
}

}  // namespace metrics
