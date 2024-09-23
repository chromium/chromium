// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/supervised_user/core/browser/proto_fetcher_metrics.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

const char kHistogramNameForRetryCount[] =
    "FamilyLinkUser.RetryTestMetrics.RetryCount";
constexpr FetcherConfig kFetcherTestConfig{
    .histogram_basename = "FamilyLinkUser.TestMetrics",
};
constexpr FetcherConfig kRetryingFetcherTestConfig{
    .histogram_basename = "FamilyLinkUser.RetryTestMetrics",
    .backoff_policy = net::BackoffEntry::Policy{},
};

// Test for metrics constructor.
class ProtoFetcherMetricsConfigTest : public testing::Test {};
TEST_F(ProtoFetcherMetricsConfigTest, EmptyFetcherConfig) {
  FetcherConfig empty_config;
  EXPECT_FALSE(ProtoFetcherMetrics::FromConfig(empty_config).has_value());
}

class CumulativeProtoFetcherMetricsConfigTest : public testing::Test {};
TEST_F(CumulativeProtoFetcherMetricsConfigTest, EmptyFetcherConfig) {
  FetcherConfig empty_config;
  EXPECT_FALSE(
      CumulativeProtoFetcherMetrics::FromConfig(empty_config).has_value());
}

TEST_F(CumulativeProtoFetcherMetricsConfigTest, BackoffPolicyEmpty) {
  EXPECT_FALSE(CumulativeProtoFetcherMetrics::FromConfig(kFetcherTestConfig)
                   .has_value());
}

// Tests the functionality of metrics helper methods.
class ProtoFetcherMetricsTest : public testing::TestWithParam<FetcherConfig> {
 public:
  ProtoFetcherMetricsTest()
      : metrics_(IsRetryingFetcher()
                     ? CumulativeProtoFetcherMetrics::FromConfig(GetParam())
                     : ProtoFetcherMetrics::FromConfig(GetParam())) {}

  bool IsRetryingFetcher() { return GetParam().backoff_policy.has_value(); }

  std::string GetHistogramNameForStatus() {
    std::string suffix = IsRetryingFetcher() ? ".OverallStatus" : ".Status";
    return base::StrCat({GetParam().histogram_basename.value(), suffix});
  }

  std::string GetHistogramNameForLatency() {
    std::string suffix = IsRetryingFetcher() ? ".OverallLatency" : ".Latency";
    return base::StrCat({GetParam().histogram_basename.value(), suffix});
  }

  std::string GetHistogramNameForStatusLatency(std::string_view status) {
    return base::StrCat(
        {GetParam().histogram_basename.value(), ".", status, ".Latency"});
  }

  std::string GetHistogramNameForAuthError() {
    return base::StrCat({GetParam().histogram_basename.value(), ".AuthError"});
  }

  std::string GetHistogramNameForHttpStatusOrNetError() {
    return base::StrCat(
        {GetParam().histogram_basename.value(), ".HttpStatusOrNetError"});
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::optional<ProtoFetcherMetrics> metrics_;
};

TEST_P(ProtoFetcherMetricsTest, RecordStatus) {
  base::HistogramTester histogram_tester;
  metrics_->RecordMetrics(ProtoFetcherStatus::Ok());
  histogram_tester.ExpectBucketCount(GetHistogramNameForStatus(),
                                     ProtoFetcherStatus::State::OK, 1);
  metrics_->RecordMetrics(ProtoFetcherStatus::InvalidResponse());
  histogram_tester.ExpectBucketCount(
      GetHistogramNameForStatus(), ProtoFetcherStatus::State::INVALID_RESPONSE,
      1);
  histogram_tester.ExpectTotalCount(GetHistogramNameForStatus(), 2);
}

TEST_P(ProtoFetcherMetricsTest, RecordLatency) {
  base::HistogramTester histogram_tester;
  task_environment_.AdvanceClock(base::Milliseconds(30));
  metrics_->RecordMetrics(ProtoFetcherStatus::Ok());
  histogram_tester.ExpectTimeBucketCount(GetHistogramNameForLatency(),
                                         base::Milliseconds(30), 1);
  task_environment_.AdvanceClock(base::Milliseconds(30));
  metrics_->RecordMetrics(ProtoFetcherStatus::Ok());
  histogram_tester.ExpectTimeBucketCount(GetHistogramNameForLatency(),
                                         base::Milliseconds(60), 1);
}

TEST_P(ProtoFetcherMetricsTest, RecordAuthError) {
  // Retrying fetcher does not support auth error metrics.
  int expected_bucket_count = IsRetryingFetcher() ? 0 : 1;
  base::HistogramTester histogram_tester;
  const GoogleServiceAuthError invalid_gaia_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
  metrics_->RecordMetrics(
      ProtoFetcherStatus::GoogleServiceAuthError(invalid_gaia_error));
  histogram_tester.ExpectBucketCount(GetHistogramNameForAuthError(),
                                     invalid_gaia_error.state(),
                                     expected_bucket_count);
  const GoogleServiceAuthError connection_failed_error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED);
  metrics_->RecordMetrics(
      ProtoFetcherStatus::GoogleServiceAuthError(connection_failed_error));
  histogram_tester.ExpectBucketCount(GetHistogramNameForAuthError(),
                                     connection_failed_error.state(),
                                     expected_bucket_count);
}

TEST_P(ProtoFetcherMetricsTest, RecordHttpStatusOrNetError) {
  // Retrying fetcher does not support HTTP status or net error metrics.
  int expected_bucket_count = IsRetryingFetcher() ? 0 : 1;
  base::HistogramTester histogram_tester;
  metrics_->RecordMetrics(
      ProtoFetcherStatus::HttpStatusOrNetError(net::ERR_IO_PENDING));
  histogram_tester.ExpectBucketCount(GetHistogramNameForHttpStatusOrNetError(),
                                     net::ERR_IO_PENDING,
                                     expected_bucket_count);
  metrics_->RecordMetrics(
      ProtoFetcherStatus::HttpStatusOrNetError(net::HTTP_BAD_REQUEST));
  histogram_tester.ExpectBucketCount(GetHistogramNameForHttpStatusOrNetError(),
                                     net::HTTP_BAD_REQUEST,
                                     expected_bucket_count);
}

TEST_P(ProtoFetcherMetricsTest, RecordStatusLatency) {
  const ProtoFetcherStatus ok_status = ProtoFetcherStatus::Ok();
  // Retrying fetcher does not support Status Latency metrics.
  int expected_bucket_count = IsRetryingFetcher() ? 0 : 1;
  base::HistogramTester histogram_tester;
  task_environment_.AdvanceClock(base::Milliseconds(30));
  metrics_->RecordMetrics(ok_status);
  histogram_tester.ExpectTimeBucketCount(
      GetHistogramNameForStatusLatency("NoError"), base::Milliseconds(30),
      expected_bucket_count);
  task_environment_.AdvanceClock(base::Milliseconds(30));
  metrics_->RecordMetrics(ok_status);
  histogram_tester.ExpectTimeBucketCount(
      GetHistogramNameForStatusLatency("NoError"), base::Milliseconds(60),
      expected_bucket_count);
  // The timer is not attached to the status, expect time to increase.
  task_environment_.AdvanceClock(base::Milliseconds(30));
  metrics_->RecordMetrics(ProtoFetcherStatus::InvalidResponse());
  histogram_tester.ExpectTimeBucketCount(
      GetHistogramNameForStatusLatency("ParseError"), base::Milliseconds(90),
      expected_bucket_count);
}

INSTANTIATE_TEST_SUITE_P(,
                         ProtoFetcherMetricsTest,
                         testing::Values(kFetcherTestConfig,
                                         kRetryingFetcherTestConfig),
                         [](const auto& info) {
                           return std::string(
                               info.param.backoff_policy.has_value()
                                   ? "RetryingFetcher"
                                   : "ProtoFetcher");
                         });

class CumulativeProtoFetcherMetricsTest : public testing::Test {
 public:
  CumulativeProtoFetcherMetricsTest()
      : metrics_(CumulativeProtoFetcherMetrics::FromConfig(
            kRetryingFetcherTestConfig)) {}

 protected:
  std::optional<CumulativeProtoFetcherMetrics> metrics_;
};

TEST_F(CumulativeProtoFetcherMetricsTest, RecordRetryCount) {
  base::HistogramTester histogram_tester;
  metrics_->RecordMetrics(ProtoFetcherStatus::Ok());
  // Retry is tracked separately from general metrics.
  histogram_tester.ExpectTotalCount(kHistogramNameForRetryCount, 0);

  metrics_->RecordRetryCount(0);
  histogram_tester.ExpectBucketCount(kHistogramNameForRetryCount, 0, 1);
  metrics_->RecordRetryCount(1);
  histogram_tester.ExpectBucketCount(kHistogramNameForRetryCount, 1, 1);
  // Maximum count is 100.
  metrics_->RecordRetryCount(101);
  histogram_tester.ExpectBucketCount(kHistogramNameForRetryCount, 100, 1);
  histogram_tester.ExpectTotalCount(kHistogramNameForRetryCount, 3);
}

}  // namespace
}  // namespace supervised_user
