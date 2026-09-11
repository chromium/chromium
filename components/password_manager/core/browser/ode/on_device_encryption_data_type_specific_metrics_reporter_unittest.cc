// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kTestHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Test";
constexpr char kTestPrefTime[] = "password_manager.test_last_reported_time";
constexpr char kTestPrefBucket[] = "password_manager.test_last_reported_bucket";

class OnDeviceEncryptionDataTypeSpecificMetricsReporterTest
    : public testing::Test {
 public:
  OnDeviceEncryptionDataTypeSpecificMetricsReporterTest() {
    pref_service_.registry()->RegisterTimePref(kTestPrefTime, base::Time());
    pref_service_.registry()->RegisterIntegerPref(kTestPrefBucket,
                                                  kNoReportedBucket);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       InitialStateRecordedAfterDelayIfAvailable) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // No initial metrics recorded before delay.
  histogram_tester_.ExpectTotalCount(kTestHistogram, 0);

  // Advance past the delay so observation starts.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       StateChangesBeforeDelayDoNotPrematurelyPublish) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_tracker = tracker.get();

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Simulate transitional state changes during startup (e.g. at 5s and 10s).
  task_environment_.FastForwardBy(base::Seconds(5));
  raw_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceNotReady);

  task_environment_.FastForwardBy(base::Seconds(5));
  raw_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  // Observations have not started yet, so no metrics are recorded.
  histogram_tester_.ExpectTotalCount(kTestHistogram, 0);

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay -
                                  base::Seconds(10));

  // Steady-state is recorded exactly once without duplicate publishing.
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       InitialStateNotAvailableDoesNotRecord) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectTotalCount(kTestHistogram, 0);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       StateTransitionsRecordedAfterDelay) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_tracker = tracker.get();

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectTotalCount(kTestHistogram, 0);

  // Transition tracker from NotAvailable -> DeviceNotReady.
  raw_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceNotReady);
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceNotReady,
      1);

  // Transition tracker from DeviceNotReady -> DeviceReady.
  raw_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectBucketCount(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);

  // Transition tracker to the PasswordAndPasskeySyncDisabled state.
  raw_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);
  histogram_tester_.ExpectBucketCount(
      kTestHistogram,
      OnDeviceEncryptionStateHistogramBucket::kPasswordAndPasskeySyncDisabled,
      1);
}

struct StateTransitionTestCase {
  std::string test_name;
  OnDeviceEncryptionState state;
  std::optional<OnDeviceEncryptionStateHistogramBucket> expected_bucket;
};

class OnDeviceEncryptionDataTypeSpecificMetricsReporterStateTransitionTest
    : public OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
      public testing::WithParamInterface<StateTransitionTestCase> {};

TEST_P(OnDeviceEncryptionDataTypeSpecificMetricsReporterStateTransitionTest,
       PublishesExpectedBucket) {
  const StateTransitionTestCase& test_case = GetParam();

  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_tracker = tracker.get();

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  // Transition tracker to the target state.
  raw_tracker->SetStateForTesting(test_case.state);

  if (test_case.expected_bucket.has_value()) {
    histogram_tester_.ExpectUniqueSample(kTestHistogram,
                                         *test_case.expected_bucket, 1);
  } else {
    histogram_tester_.ExpectTotalCount(kTestHistogram, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OnDeviceEncryptionDataTypeSpecificMetricsReporterStateTransitionTest,
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
                kPasswordAndPasskeySyncDisabled},
        StateTransitionTestCase{
            .test_name = "ProfileNotSignedIn",
            .state = OnDeviceEncryptionState::kProfileNotSignedIn,
            .expected_bucket =
                OnDeviceEncryptionStateHistogramBucket::kProfileNotSignedIn},
        StateTransitionTestCase{
            .test_name = "ProfileSignInPending",
            .state = OnDeviceEncryptionState::kProfileSignInPending,
            .expected_bucket =
                OnDeviceEncryptionStateHistogramBucket::kProfileSignInPending}),
    [](const testing::TestParamInfo<StateTransitionTestCase>& info) {
      return info.param.test_name;
    });

// Not all OnDeviceEncryptionState values are published to UMA (e.g.,
// kOnDeviceEncryptionStateNotAvailable is omitted). In case of a state
// transition A -> B -> A, where B is a state that is not published to UMA,
// the state A should only be published to UMA once.
TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       DoesNotPublishDuplicateMetricWhenCyclingThroughNotAvailable) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_tracker = tracker.get();

  raw_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance the time to start observations.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectUniqueSample(
      kTestHistogram,
      OnDeviceEncryptionStateHistogramBucket::kOnDeviceEncryptionNotEnabled, 1);

  // Transition to kOnDeviceEncryptionStateNotAvailable (not published to UMA).
  raw_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
  histogram_tester_.ExpectTotalCount(kTestHistogram, 1);

  // Transition back to kOnDeviceEncryptionNotEnabled (same bucket as before).
  raw_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  // Should not publish duplicate metrics; counts remain 1.
  histogram_tester_.ExpectTotalCount(kTestHistogram, 1);

  // Transition to a different bucket (kDeviceReady).
  raw_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  // Should publish since the bucket changed.
  histogram_tester_.ExpectTotalCount(kTestHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       PeriodicReporting) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Fast forward past initial delay. Initial sample recorded.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);

  // Advance by periodic reporting delay. Metric is recorded again.
  task_environment_.FastForwardBy(kPeriodicReportingDelay);
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 2);

  // Advance another period. Metric is recorded periodically again.
  task_environment_.FastForwardBy(kPeriodicReportingDelay);
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 3);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       SkipsInitialReportingIfAlreadyReportedRecently) {
  // Pre-populate prefs: reported 1 hour ago with kDeviceReady.
  base::Time report_time = base::Time::Now() - base::Hours(1);
  pref_service_.SetTime(kTestPrefTime, report_time);
  pref_service_.SetInteger(
      kTestPrefBucket,
      static_cast<int>(OnDeviceEncryptionStateHistogramBucket::kDeviceReady));

  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance past initial delay.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  // Should NOT publish on startup because it was already reported recently.
  histogram_tester_.ExpectTotalCount(kTestHistogram, 0);

  // Advance by (kPeriodicReportingDelay - 1 hour) for reaching the periodic
  // reporting threshold.
  task_environment_.FastForwardBy(kPeriodicReportingDelay - base::Hours(1));
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       PublishesImmediatelyWhenStateChangesAndAdjustsNextReportingTime) {
  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_tracker = tracker.get();
  tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance past initial delay. Initial sample recorded.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);

  // Advance 2 hours. State changes to kPasswordAndPasskeySyncDisabled.
  task_environment_.FastForwardBy(base::Hours(2));
  raw_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);

  // Should publish immediately because bucket changed.
  histogram_tester_.ExpectTotalCount(kTestHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      kTestHistogram,
      OnDeviceEncryptionStateHistogramBucket::kPasswordAndPasskeySyncDisabled,
      1);

  // Next scheduled reporting should happen only after kPeriodicReportingDelay.

  // Advancing by (kPeriodicReportingDelay - 1 hour) should not trigger
  // reporting.
  task_environment_.FastForwardBy(kPeriodicReportingDelay - base::Hours(1));
  histogram_tester_.ExpectTotalCount(kTestHistogram, 2);
  // Advancing by 1 hour afterwards - should trigger reporting.
  task_environment_.FastForwardBy(base::Hours(1));
  histogram_tester_.ExpectTotalCount(kTestHistogram, 3);
  histogram_tester_.ExpectBucketCount(
      kTestHistogram,
      OnDeviceEncryptionStateHistogramBucket::kPasswordAndPasskeySyncDisabled,
      2);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       PublishesMetricWhenLastReportedTimeIsInTheFuture) {
  // Pre-populate prefs: last reported time is in the future (e.g. clock
  // change), with the same bucket (kDeviceReady).
  const base::Time future_report_time = base::Time::Now() + base::Hours(5);
  pref_service_.SetTime(kTestPrefTime, future_report_time);
  pref_service_.SetInteger(
      kTestPrefBucket,
      static_cast<int>(OnDeviceEncryptionStateHistogramBucket::kDeviceReady));

  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  // Advance past initial delay.
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  // Even though bucket is unchanged and "reported recently" in the future,
  // detecting a reporting time mismatch (now < last_reported_time) should
  // trigger publication to heal the state.
  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);

  // The last reported time in preferences should be corrected to current time.
  EXPECT_EQ(pref_service_.GetTime(kTestPrefTime), base::Time::Now());

  // Subsequent periodic report should be scheduled kPeriodicReportingDelay from
  // now.
  task_environment_.FastForwardBy(kPeriodicReportingDelay - base::Hours(1));
  histogram_tester_.ExpectTotalCount(kTestHistogram, 1);

  task_environment_.FastForwardBy(base::Hours(1));
  histogram_tester_.ExpectTotalCount(kTestHistogram, 2);
}

TEST_F(OnDeviceEncryptionDataTypeSpecificMetricsReporterTest,
       PublishesMetricWhenLastReportedBucketIsOutOfRange) {
  pref_service_.SetTime(kTestPrefTime, base::Time::Now());
  pref_service_.SetInteger(kTestPrefBucket, 1000);

  auto tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter reporter(
      kTestHistogram, kTestPrefTime, kTestPrefBucket, std::move(tracker),
      pref_service_);

  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  histogram_tester_.ExpectUniqueSample(
      kTestHistogram, OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

}  // namespace

}  // namespace password_manager
