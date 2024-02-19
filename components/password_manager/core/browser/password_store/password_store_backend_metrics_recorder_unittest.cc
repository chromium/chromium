// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
using testing::ElementsAre;
using testing::IsEmpty;

using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

constexpr auto kLatencyDelta = base::Milliseconds(123u);

constexpr char kSomeBackend[] = "SomeBackend";
constexpr char kSomeMethod[] = "MethodName";

constexpr char kSpecificMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName";
constexpr char kDurationMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.Latency";
constexpr char kSuccessMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.Success";
constexpr char kErrorCodeMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.ErrorCode";
constexpr char kApiErrorMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.APIError";
constexpr char kConnectionResultMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.ConnectionResultCode";

// Metrics including the store infix.
constexpr char kSpecificMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.MethodName";
constexpr char kDurationMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.MethodName.Latency";
constexpr char kSuccessMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.MethodName.Success";
constexpr char kOverallSuccessMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.Success";
constexpr char kErrorCodeMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.MethodName.ErrorCode";
constexpr char kOverallErrorCodeMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.ErrorCode";
constexpr char kApiErrorMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.MethodName.APIError";
constexpr char kOverallApiErrorMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.APIError";
constexpr char kConnectionResultMetricWithStoreInfix[] =
    "PasswordManager.PasswordStoreSomeBackend.Account.MethodName."
    "ConnectionResultCode";

// Overall metrics.
constexpr char kOverallMetric[] =
    "PasswordManager.PasswordStoreBackend.MethodName";
constexpr char kDurationOverallMetric[] =
    "PasswordManager.PasswordStoreBackend.MethodName.Latency";
constexpr char kSuccessOverallMetric[] =
    "PasswordManager.PasswordStoreBackend.MethodName.Success";
constexpr char kErrorCodeOverallMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
constexpr char kApiErrorOverallMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.APIError";
constexpr char kConnectionResultOverallMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.ConnectionResultCode";
}  // anonymous namespace

class PasswordStoreBackendMetricsRecorderTest
    : public testing::TestWithParam<PasswordStoreBackendMetricsRecorder::
                                        PasswordStoreAndroidBackendType> {
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

TEST_P(PasswordStoreBackendMetricsRecorderTest, RecordMetrics_Success) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  bool has_store_type = GetParam() !=
                        PasswordStoreBackendMetricsRecorder::
                            PasswordStoreAndroidBackendType::kNone;
  PasswordStoreBackendMetricsRecorder metrics_recorder;
  if (has_store_type) {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod), GetParam());
  } else {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod),
        PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
            kNone);
  }

  // Checking started requests in the overall and backend-specific histogram.
  EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetric),
              ElementsAre(Bucket(/* Requested */ 0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kOverallMetric),
              ElementsAre(Bucket(/* Requested */ 0, 1)));
  // Checking started the store-type-specific histogram.
  if (has_store_type) {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                ElementsAre(Bucket(/* Requested */ 0, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                IsEmpty());
  }

  AdvanceClock(kLatencyDelta);

  metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                 /*error=*/std::nullopt);

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(true, 1)));

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationOverallMetric, kLatencyDelta,
                                         1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(true, 1)));

  // Checking completed requests in the overall and backend-specific histogram.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSpecificMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Completed */ 2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kOverallMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Completed */ 2, 1)));

  // Checking the store-type-specific histograms.
  if (has_store_type) {
    histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 1);
    histogram_tester.ExpectTimeBucketCount(kDurationMetricWithStoreInfix,
                                           kLatencyDelta, 1);
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                ElementsAre(Bucket(true, 1)));
    histogram_tester.ExpectUniqueSample(kOverallSuccessMetricWithStoreInfix,
                                        true, 1);

    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                ElementsAre(Bucket(/* Requested */ 0, 1),
                            Bucket(/* Completed */ 2, 1)));
  } else {
    histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 0);
    histogram_tester.ExpectTimeBucketCount(kDurationMetricWithStoreInfix,
                                           kLatencyDelta, 0);
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                IsEmpty());
    histogram_tester.ExpectUniqueSample(kOverallSuccessMetricWithStoreInfix,
                                        true, 0);

    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                IsEmpty());
  }
}

TEST_P(PasswordStoreBackendMetricsRecorderTest, RecordMetrics_ExternalError) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  bool has_store_type = GetParam() !=
                        PasswordStoreBackendMetricsRecorder::
                            PasswordStoreAndroidBackendType::kNone;
  PasswordStoreBackendMetricsRecorder metrics_recorder;
  if (has_store_type) {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod), GetParam());
  } else {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod),
        PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
            kNone);
  }

  AdvanceClock(kLatencyDelta);

  AndroidBackendError error(AndroidBackendErrorType::kExternalError);
  error.api_error_code = 11010;
  metrics_recorder.RecordMetrics(SuccessStatus::kError, std::move(error));

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetric),
              ElementsAre(Bucket(7, 1)));  // External
  EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetric),
              ElementsAre(Bucket(11010, 1)));  // No access.
  EXPECT_THAT(histogram_tester.GetAllSamples(kConnectionResultMetric),
              IsEmpty());
  // Checking records in the store-type-specific histograms.
  if (has_store_type) {
    histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 1);
    histogram_tester.ExpectTimeBucketCount(kDurationMetricWithStoreInfix,
                                           kLatencyDelta, 1);
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                ElementsAre(Bucket(false, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetricWithStoreInfix),
                ElementsAre(Bucket(7, 1)));  // External
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallErrorCodeMetricWithStoreInfix),
        ElementsAre(Bucket(7, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetricWithStoreInfix),
                ElementsAre(Bucket(11010, 1)));  // No access.
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallApiErrorMetricWithStoreInfix),
        ElementsAre(Bucket(11010, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kConnectionResultMetricWithStoreInfix),
        IsEmpty());
  } else {
    histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 0);
    histogram_tester.ExpectTimeBucketCount(kDurationMetricWithStoreInfix,
                                           kLatencyDelta, 0);
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetricWithStoreInfix),
                IsEmpty());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallErrorCodeMetricWithStoreInfix),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetricWithStoreInfix),
                IsEmpty());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallApiErrorMetricWithStoreInfix),
        IsEmpty());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kConnectionResultMetricWithStoreInfix),
        IsEmpty());
  }

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationOverallMetric, kLatencyDelta,
                                         1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeOverallMetric),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorOverallMetric),
              ElementsAre(Bucket(11010, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kConnectionResultOverallMetric),
              IsEmpty());

  // Checking completed requests in the overall and backend-specific histogram.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSpecificMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Completed */ 2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kOverallMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Completed */ 2, 1)));
  // Checking completed requests in the store-type-specific histograms.
  if (has_store_type) {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                ElementsAre(Bucket(/* Requested */ 0, 1),
                            Bucket(/* Completed */ 2, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                IsEmpty());
  }
}

TEST_P(PasswordStoreBackendMetricsRecorderTest,
       RecordMetrics_ExternalErrorWithConnectionResult) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  const int kApiUnavailableConnectionResult = 16;

  bool has_store_type = GetParam() !=
                        PasswordStoreBackendMetricsRecorder::
                            PasswordStoreAndroidBackendType::kNone;
  PasswordStoreBackendMetricsRecorder metrics_recorder;
  if (has_store_type) {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod), GetParam());
  } else {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod),
        PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
            kNone);
  }

  AdvanceClock(kLatencyDelta);

  AndroidBackendError error(AndroidBackendErrorType::kExternalError);
  error.api_error_code = 11010;
  error.connection_result_code = kApiUnavailableConnectionResult;
  metrics_recorder.RecordMetrics(SuccessStatus::kError, std::move(error));

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetric),
              ElementsAre(Bucket(7, 1)));  // External
  EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetric),
              ElementsAre(Bucket(11010, 1)));  // No access.
  EXPECT_THAT(histogram_tester.GetAllSamples(kConnectionResultMetric),
              ElementsAre(Bucket(kApiUnavailableConnectionResult, 1)));
  // Checking records in the store-type-specific histograms.
  if (has_store_type) {
    histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 1);
    histogram_tester.ExpectTimeBucketCount(kDurationMetricWithStoreInfix,
                                           kLatencyDelta, 1);
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                ElementsAre(Bucket(false, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetricWithStoreInfix),
                ElementsAre(Bucket(7, 1)));  // External
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallErrorCodeMetricWithStoreInfix),
        ElementsAre(Bucket(7, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetricWithStoreInfix),
                ElementsAre(Bucket(11010, 1)));  // No access.
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallApiErrorMetricWithStoreInfix),
        ElementsAre(Bucket(11010, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kConnectionResultMetricWithStoreInfix),
        ElementsAre(Bucket(kApiUnavailableConnectionResult, 1)));
  } else {
    histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 0);
    histogram_tester.ExpectTimeBucketCount(kDurationMetricWithStoreInfix,
                                           kLatencyDelta, 0);
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetricWithStoreInfix),
                IsEmpty());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallErrorCodeMetricWithStoreInfix),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetricWithStoreInfix),
                IsEmpty());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kOverallApiErrorMetricWithStoreInfix),
        IsEmpty());
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kConnectionResultMetricWithStoreInfix),
        IsEmpty());
  }

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationOverallMetric, kLatencyDelta,
                                         1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeOverallMetric),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorOverallMetric),
              ElementsAre(Bucket(11010, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kConnectionResultOverallMetric),
              ElementsAre(Bucket(kApiUnavailableConnectionResult, 1)));

  // Checking completed requests in the overall and backend-specific histogram.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSpecificMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Completed */ 2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kOverallMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Completed */ 2, 1)));
  // Checking completed requests in the store-type-specific histogram.
  if (has_store_type) {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                ElementsAre(Bucket(/* Requested */ 0, 1),
                            Bucket(/* Completed */ 2, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                IsEmpty());
  }
}

TEST_P(PasswordStoreBackendMetricsRecorderTest,
       RecordMetrics_CancelledTimeout) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  bool has_store_type = GetParam() !=
                        PasswordStoreBackendMetricsRecorder::
                            PasswordStoreAndroidBackendType::kNone;
  PasswordStoreBackendMetricsRecorder metrics_recorder;
  if (has_store_type) {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod), GetParam());
  } else {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod),
        PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
            kNone);
  }

  AdvanceClock(kLatencyDelta);

  metrics_recorder.RecordMetrics(SuccessStatus::kCancelledTimeout,
                                 /*error=*/std::nullopt);

  // Checking records in the backend-specific histogram.
  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(false, 1)));
  // Checking records in the store-type-specific histograms.
  histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 0);
  if (has_store_type) {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                ElementsAre(Bucket(false, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                IsEmpty());
  }

  // Checking records in the overall histogram.
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(false, 1)));

  // Checking timed-out requests in the overall and backend-specific histogram.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kSpecificMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Timeout */ 1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kOverallMetric),
      ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Timeout */ 1, 1)));
  // Checking timed-out requests in the store-type-specific histograms.
  if (has_store_type) {
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
        ElementsAre(Bucket(/* Requested */ 0, 1), Bucket(/* Timeout */ 1, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                IsEmpty());
  }
}

TEST_P(PasswordStoreBackendMetricsRecorderTest,
       RecordMetrics_CancelledPwdSyncStateChanged) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  bool has_store_type = GetParam() !=
                        PasswordStoreBackendMetricsRecorder::
                            PasswordStoreAndroidBackendType::kNone;
  PasswordStoreBackendMetricsRecorder metrics_recorder;
  if (has_store_type) {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod), GetParam());
  } else {
    metrics_recorder = PasswordStoreBackendMetricsRecorder(
        BackendInfix(kSomeBackend), MethodName(kSomeMethod),
        PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
            kNone);
  }

  AdvanceClock(kLatencyDelta);

  metrics_recorder.RecordMetrics(SuccessStatus::kCancelledPwdSyncStateChanged,
                                 /*error=*/std::nullopt);

  // Checking records in the backend-specific histogram.
  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(false, 1)));
  // Checking records in the store-type-specific histograms.
  histogram_tester.ExpectTotalCount(kDurationMetricWithStoreInfix, 0);
  if (has_store_type) {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                ElementsAre(Bucket(false, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetricWithStoreInfix),
                IsEmpty());
  }

  // Checking records in the overall histogram.
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(false, 1)));

  // Checking timed-out requests in the overall and backend-specific histogram.
  EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetric),
              ElementsAre(Bucket(/* Requested */ 0, 1),
                          Bucket(/* CancelledPwdSyncStateChanged */ 3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kOverallMetric),
              ElementsAre(Bucket(/* Requested */ 0, 1),
                          Bucket(/* CancelledPwdSyncStateChanged */ 3, 1)));
  // Checking timed-out requests in the store-type-specific histograms.
  if (has_store_type) {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                ElementsAre(Bucket(/* Requested */ 0, has_store_type ? 1 : 0),
                            Bucket(/* CancelledPwdSyncStateChanged */ 3, 1)));
  } else {
    EXPECT_THAT(histogram_tester.GetAllSamples(kSpecificMetricWithStoreInfix),
                IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordStoreBackendMetricsRecorderTest,
    testing::Values(PasswordStoreBackendMetricsRecorder::
                        PasswordStoreAndroidBackendType::kNone,
                    PasswordStoreBackendMetricsRecorder::
                        PasswordStoreAndroidBackendType::kAccount));

}  // namespace password_manager
