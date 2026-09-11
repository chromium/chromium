// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class OnDeviceEncryptionMetricsReporterTest : public testing::Test {
 public:
  OnDeviceEncryptionMetricsReporterTest() {
    OnDeviceEncryptionMetricsReporter::RegisterProfilePrefs(
        pref_service_.registry());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       InitialStateRecordedAfterDelayIfAvailable) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  OnDeviceEncryptionMetricsReporter reporter(
      std::move(passkey_tracker), std::move(password_tracker), pref_service_);

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
       StateTransitionsRecordedAfterDelay) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_passkey_tracker = passkey_tracker.get();

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_password_tracker = password_tracker.get();

  OnDeviceEncryptionMetricsReporter reporter(
      std::move(passkey_tracker), std::move(password_tracker), pref_service_);

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

}  // namespace

}  // namespace password_manager
