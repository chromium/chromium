// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"

#include <array>
#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace password_manager {

namespace {

constexpr char kPasskeyOnDeviceEncryptionStateHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Passkeys";
constexpr char kPasswordOnDeviceEncryptionStateHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Passwords";
constexpr char kPasskeyOnDeviceEncryptionTimeToUnlockHistogram[] =
    "PasswordManager.OnDeviceEncryptionTimeToUnlock.Passkeys";
constexpr char kPasswordOnDeviceEncryptionTimeToUnlockHistogram[] =
    "PasswordManager.OnDeviceEncryptionTimeToUnlock.Passwords";

constexpr std::array<const char*, 5> kTimeToUnlockPrefs = {
    prefs::kOnDeviceEncryptionLastSignInTime,
    prefs::kOnDeviceEncryptionPasskeyFirstUnlockTime,
    prefs::kOnDeviceEncryptionPasskeyTimeToUnlockLastReportingTime,
    prefs::kOnDeviceEncryptionPasswordFirstUnlockTime,
    prefs::kOnDeviceEncryptionPasswordTimeToUnlockLastReportingTime,
};

class OnDeviceEncryptionMetricsReporterTest : public testing::Test {
 public:
  OnDeviceEncryptionMetricsReporterTest() {
    OnDeviceEncryptionMetricsReporter::RegisterProfilePrefs(
        pref_service_.registry());
  }

 protected:
  std::unique_ptr<OnDeviceEncryptionMetricsReporter>
  CreateReporterAndInitializeTrackers(
      OnDeviceEncryptionStateTracker** raw_passkey_tracker = nullptr,
      OnDeviceEncryptionStateTracker** raw_password_tracker = nullptr,
      signin::IdentityManager* identity_manager = nullptr) {
    auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
    if (raw_passkey_tracker) {
      *raw_passkey_tracker = passkey_tracker.get();
    }
    auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
    if (raw_password_tracker) {
      *raw_password_tracker = password_tracker.get();
    }
    return std::make_unique<OnDeviceEncryptionMetricsReporter>(
        std::move(passkey_tracker), std::move(password_tracker), &pref_service_,
        identity_manager ? identity_manager
                         : identity_test_env_.identity_manager());
  }

  void SeedTimeToUnlockPrefs(base::Time time) {
    for (const char* pref : kTimeToUnlockPrefs) {
      pref_service_.SetTime(pref, time);
    }
  }

  void ExpectAllTimeToUnlockPrefsCleared() {
    for (const char* pref : kTimeToUnlockPrefs) {
      SCOPED_TRACE(pref);
      EXPECT_FALSE(pref_service_.HasPrefPath(pref));
    }
  }

  void ExpectAllTimeToUnlockPrefsEqual(base::Time expected_time) {
    for (const char* pref : kTimeToUnlockPrefs) {
      SCOPED_TRACE(pref);
      EXPECT_EQ(pref_service_.GetTime(pref), expected_time);
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       InitialStateRecordedAfterDelayIfAvailable) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  OnDeviceEncryptionMetricsReporter reporter(
      std::move(passkey_tracker), std::move(password_tracker), &pref_service_,
      identity_test_env_.identity_manager());

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
      std::move(passkey_tracker), std::move(password_tracker), &pref_service_,
      identity_test_env_.identity_manager());

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

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       SignInAndUnlockRecordsTimeToUnlock) {
  OnDeviceEncryptionStateTracker* passkey_tracker = nullptr;
  OnDeviceEncryptionStateTracker* password_tracker = nullptr;
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers(&passkey_tracker, &password_tracker);

  task_environment_.FastForwardBy(kInitialStateReportingDelay);
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  EXPECT_EQ(pref_service_.GetTime(prefs::kOnDeviceEncryptionLastSignInTime),
            base::Time::Now());

  task_environment_.FastForwardBy(base::Minutes(2));
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 120, 1);
  histogram_tester_.ExpectTotalCount(
      kPasswordOnDeviceEncryptionTimeToUnlockHistogram, 0);

  task_environment_.FastForwardBy(base::Minutes(3));
  password_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionTimeToUnlockHistogram, 300, 1);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       SignInBeforeInitialDelayWaitsUntilDelayBeforeRecording) {
  OnDeviceEncryptionStateTracker* passkey_tracker = nullptr;
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers(&passkey_tracker);

  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectTotalCount(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 0);

  task_environment_.FastForwardBy(kInitialStateReportingDelay);
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram,
      kInitialStateReportingDelay.InSeconds(), 1);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       SignedInStartupPreservesTimeToUnlockPrefs) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  const base::Time seeded_time = base::Time::Now() - base::Hours(1);
  SeedTimeToUnlockPrefs(seeded_time);

  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers();
  ExpectAllTimeToUnlockPrefsEqual(seeded_time);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       OnIdentityManagerShutdownClearsObservation) {
  auto local_identity_env = std::make_unique<signin::IdentityTestEnvironment>();
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers(
          nullptr, nullptr, local_identity_env->identity_manager());

  local_identity_env.reset();
  reporter->Shutdown();
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       SignedOutStartupClearsStaleTimeToUnlockPrefs) {
  SeedTimeToUnlockPrefs(base::Time::Now());
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers();
  ExpectAllTimeToUnlockPrefsCleared();
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       NoOpPrimaryAccountChangeDoesNotResetSignInOrUnlockData) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  const base::Time seeded_time = base::Time::Now() - base::Minutes(5);
  SeedTimeToUnlockPrefs(seeded_time);
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers();

  const signin::PrimaryAccountChangeEvent::State signed_in_state(
      account.GetCoreAccountInfo(), signin::ConsentLevel::kSignin);
  reporter->OnPrimaryAccountChanged(signin::PrimaryAccountChangeEvent(
      signed_in_state, signed_in_state,
      signin_metrics::AccessPoint::kSettings));

  ExpectAllTimeToUnlockPrefsEqual(seeded_time);
}

// On ChromeOS, a profile's primary account cannot be cleared or switched to a
// different account after being set.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(OnDeviceEncryptionMetricsReporterTest,
       SignOutClearsAllCachedTimeToUnlockPrefs) {
  OnDeviceEncryptionStateTracker* passkey_tracker = nullptr;
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers(&passkey_tracker);
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  SeedTimeToUnlockPrefs(base::Time::Now());

  identity_test_env_.ClearPrimaryAccount();
  ExpectAllTimeToUnlockPrefsCleared();

  // Re-signing in starts a new measurement cleanly.
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  task_environment_.FastForwardBy(base::Seconds(45));
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 45, 1);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       DirectAccountSwitchResetsUnlockStateAndMeasuresFromNewSignIn) {
  OnDeviceEncryptionStateTracker* passkey_tracker = nullptr;
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers(&passkey_tracker);
  task_environment_.FastForwardBy(kInitialStateReportingDelay);

  identity_test_env_.MakePrimaryAccountAvailable("first@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  task_environment_.FastForwardBy(base::Seconds(20));
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 20, 1);

  AccountInfo second_account = signin::MakeAccountAvailable(
      identity_test_env_.identity_manager(), "second@gmail.com");
  ASSERT_EQ(identity_test_env_.identity_manager()
                ->GetPrimaryAccountMutator()
                ->SetPrimaryAccount(second_account.GetAccountId(),
                                    signin::ConsentLevel::kSignin,
                                    signin_metrics::AccessPoint::kSettings),
            signin::PrimaryAccountMutator::PrimaryAccountError::kNoError);

  EXPECT_EQ(pref_service_.GetTime(prefs::kOnDeviceEncryptionLastSignInTime),
            base::Time::Now());
  EXPECT_FALSE(pref_service_.HasPrefPath(
      prefs::kOnDeviceEncryptionPasskeyFirstUnlockTime));
  EXPECT_FALSE(pref_service_.HasPrefPath(
      prefs::kOnDeviceEncryptionPasskeyTimeToUnlockLastReportingTime));

  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceNotReady);
  task_environment_.FastForwardBy(base::Seconds(10));
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectBucketCount(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 10, 1);
  histogram_tester_.ExpectTotalCount(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 2);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       KillswitchDisablesIdentityAndTimeToUnlockTracking) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPasswordManagerOnDeviceEncryptionTimeToUnlockMetrics);

  const base::Time seeded_time = base::Time::Now();
  SeedTimeToUnlockPrefs(seeded_time);

  auto local_identity_env = std::make_unique<signin::IdentityTestEnvironment>();
  OnDeviceEncryptionStateTracker* passkey_tracker = nullptr;
  std::unique_ptr<OnDeviceEncryptionMetricsReporter> reporter =
      CreateReporterAndInitializeTrackers(
          &passkey_tracker, nullptr, local_identity_env->identity_manager());

  task_environment_.FastForwardBy(kInitialStateReportingDelay);
  local_identity_env->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
  histogram_tester_.ExpectTotalCount(
      kPasskeyOnDeviceEncryptionTimeToUnlockHistogram, 0);
  ExpectAllTimeToUnlockPrefsEqual(seeded_time);

  local_identity_env.reset();
  reporter->Shutdown();
}

}  // namespace

}  // namespace password_manager
