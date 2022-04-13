// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
constexpr auto kLatencyDelta = base::Milliseconds(123u);
}  // anonymous namespace

class PasswordStoreBackendMetricsRecorderTest : public testing::Test {
 public:
  void AdvanceClock(base::TimeDelta millis) {
    // AdvanceClock is used here because FastForwardBy doesn't work for the
    // intended purpose. FastForwardBy performs the queued actions first and
    // then makes the clock tick and for the tests that follow we want to
    // advance the clock before certain async tasks happen.
    task_environment_.AdvanceClock(millis);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PasswordStoreBackendMetricsRecorderTest, RecordMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreSomeBackend.MethodName.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreSomeBackend.MethodName.Success";
  const char kDurationOverallMetric[] =
      "PasswordManager.PasswordStoreBackend.MethodName.Latency";
  const char kSuccessOverallMetric[] =
      "PasswordManager.PasswordStoreBackend.MethodName.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix("SomeBackend"),
                                          MetricInfix("MethodName"));

  AdvanceClock(kLatencyDelta);

  metrics_recorder.RecordMetrics(/*success=*/true, /*error=*/absl::nullopt);

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationOverallMetric, kLatencyDelta,
                                         1);
  histogram_tester.ExpectTotalCount(kSuccessOverallMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessOverallMetric, true, 1);
}

}  // namespace password_manager
