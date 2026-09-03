// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class OnDeviceEncryptionMetricsReporterTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       InitialStateRecordedAfterDelayIfAvailable) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  // No initial metrics recorded before delay.
  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     0);

  // Advance past the delay so observation starts.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kOnDeviceEncryptionNotEnabled, 1);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       StateChangesBeforeDelayDoNotPrematurelyPublish) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_passkey_tracker = passkey_tracker.get();

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_password_tracker = password_tracker.get();

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  // Simulate transitional state changes during startup (e.g. at 5s and 10s).
  task_environment_.FastForwardBy(base::Seconds(5));
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceNotReady);
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceNotReady);

  task_environment_.FastForwardBy(base::Seconds(5));
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceReady);
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceReady);

  // Observations have not started yet, so no metrics are recorded.
  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     0);

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay -
                                  base::Seconds(10));

  // Steady-state is recorded exactly once without duplicate publishing.
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       InitialStateNotAvailableDoesNotRecord) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     0);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       StateTransitionsRecordedAfterDelay) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_passkey_tracker = passkey_tracker.get();

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_password_tracker = password_tracker.get();

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     0);

  // Transition passkey tracker from NotAvailable -> DeviceNotReady.
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceNotReady);
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceNotReady, 1);

  // Transition password tracker from NotAvailable -> DeviceReady.
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);

  // Transition passkey tracker to the PasswordAndPasskeySyncDisabled state.
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);
  histogram_tester_.ExpectBucketCount(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kPasswordAndPasskeySyncDisabled,
      1);

  // Transition password tracker to the PasswordAndPasskeySyncDisabled state.
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);
  histogram_tester_.ExpectBucketCount(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kPasswordAndPasskeySyncDisabled,
      1);
}

struct StateTransitionTestCase {
  std::string test_name;
  OnDeviceEncryptionState state;
  std::optional<OnDeviceEncryptionStateHistogramBucket> expected_bucket;
};

class OnDeviceEncryptionMetricsReporterStateTransitionTest
    : public OnDeviceEncryptionMetricsReporterTest,
      public testing::WithParamInterface<StateTransitionTestCase> {};

TEST_P(OnDeviceEncryptionMetricsReporterStateTransitionTest,
       PublishesExpectedBucket) {
  const StateTransitionTestCase& test_case = GetParam();

  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_passkey_tracker = passkey_tracker.get();

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_password_tracker = password_tracker.get();

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  // Transition trackers to the target state.
  raw_passkey_tracker->SetStateForTesting(test_case.state);
  raw_password_tracker->SetStateForTesting(test_case.state);

  if (test_case.expected_bucket.has_value()) {
    histogram_tester_.ExpectUniqueSample(
        kPasskeyOnDeviceEncryptionStateHistogram, *test_case.expected_bucket,
        1);
    histogram_tester_.ExpectUniqueSample(
        kPasswordOnDeviceEncryptionStateHistogram, *test_case.expected_bucket,
        1);
  } else {
    histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                       0);
    histogram_tester_.ExpectTotalCount(
        kPasswordOnDeviceEncryptionStateHistogram, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OnDeviceEncryptionMetricsReporterStateTransitionTest,
    testing::Values(
        StateTransitionTestCase{
            .test_name = "OnDeviceEncryptionStateNotAvailable",
            .state =
                OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable,
            .expected_bucket = std::nullopt},
        StateTransitionTestCase{
            .test_name = "OnDeviceEncryptionNotEnabled",
            .state = OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled,
            .expected_bucket = OnDeviceEncryptionStateHistogramBucket::
                kOnDeviceEncryptionNotEnabled},
        StateTransitionTestCase{
            .test_name = "DeviceNotReady",
            .state = OnDeviceEncryptionState::kDeviceNotReady,
            .expected_bucket =
                OnDeviceEncryptionStateHistogramBucket::kDeviceNotReady},
        StateTransitionTestCase{
            .test_name = "DeviceReady",
            .state = OnDeviceEncryptionState::kDeviceReady,
            .expected_bucket =
                OnDeviceEncryptionStateHistogramBucket::kDeviceReady},
        StateTransitionTestCase{
            .test_name = "PasswordAndPasskeySyncDisabled",
            .state = OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled,
            .expected_bucket = OnDeviceEncryptionStateHistogramBucket::
                kPasswordAndPasskeySyncDisabled}),
    [](const testing::TestParamInfo<StateTransitionTestCase>& info) {
      return info.param.test_name;
    });

// Not all OnDeviceEncryptionState values are published to UMA (e.g.,
// kOnDeviceEncryptionStateNotAvailable is omitted). In case of a state
// transition A -> B -> A, where B is a state that is not published to UMA,
// the state A should only be published to UMA once.
TEST_F(OnDeviceEncryptionMetricsReporterTest,
       DoesNotPublishDuplicateMetricWhenCyclingThroughNotAvailable) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_passkey_tracker = passkey_tracker.get();

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_password_tracker = password_tracker.get();

  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kOnDeviceEncryptionNotEnabled, 1);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kOnDeviceEncryptionNotEnabled, 1);

  // Transition to kOnDeviceEncryptionStateNotAvailable (not published to UMA).
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);

  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     1);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     1);

  // Transition back to kOnDeviceEncryptionNotEnabled (same bucket as before).
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  // Should not publish duplicate metrics; counts remain 1.
  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     1);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     1);

  // Transition to a different bucket (kDeviceReady).
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceReady);
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceReady);

  // Should publish since the bucket changed.
  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     2);
  histogram_tester_.ExpectBucketCount(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     2);
  histogram_tester_.ExpectBucketCount(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

}  // namespace

}  // namespace password_manager
