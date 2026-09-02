// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/universal_optout/universal_optout_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/universal_optout/features.h"
#include "components/universal_optout/prefs.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/variations_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace universal_optout {

class UniversalOptOutServiceTest : public ::testing::Test {
 public:
  UniversalOptOutServiceTest() {
    universal_optout::prefs::RegisterProfilePrefs(pref_service_.registry());
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());

    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/true,
                                                            /*enabled=*/true);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());

    // Start clock at a fixed known time (e.g. 2026-08-11 12:00:00 UTC).
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString("2026-08-11T12:00:00Z", &start_time));
    test_clock_.SetNow(start_time);
  }

  void SetUp() override {
    pref_service_.ClearPref(prefs::kUniversalOptOutEligibilityHistory);
    pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, false);
    pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, false);
  }

  void EnableFeatureWithTargetLocations(const std::string& target_locations) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{base::test::FeatureRefAndParams(
            features::kUniversalOptOut,
            {{"target_locations", target_locations}})},
        /*disabled_features=*/{});
  }

  void SetGeoLevel1(const std::string& geo_level1) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        variations::switches::kVariationsOverrideGeoLevel1, geo_level1);
  }

  std::unique_ptr<UniversalOptOutService> CreateService() {
    return std::make_unique<UniversalOptOutService>(
        pref_service_, *variations_service_,
        *identity_test_env_.identity_manager(), test_clock_);
  }

  base::Time GetCurrentDay(base::Time time) { return time.UTCMidnight(); }

  void SetHistoricalDay(base::Time day, bool is_eligible) {
    ScopedDictPrefUpdate update(&pref_service_,
                                prefs::kUniversalOptOutEligibilityHistory);
    update->Set(base::TimeToValue(day).GetString(), is_eligible);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

TEST_F(UniversalOptOutServiceTest, FeatureDisabledByDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kUniversalOptOut);

  SetGeoLevel1("us-fl");
  auto service = CreateService();
  EXPECT_FALSE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest, EligibleWhenInTargetLocation) {
  EnableFeatureWithTargetLocations("us-fl");

  SetGeoLevel1("us-fl");
  auto service = CreateService();

  EXPECT_TRUE(service->IsEligible());
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUniversalOptOutEligible));

  const base::DictValue& history =
      pref_service_.GetDict(prefs::kUniversalOptOutEligibilityHistory);
  base::Time current_day = GetCurrentDay(test_clock_.Now());
  std::string day_key = base::TimeToValue(current_day).GetString();
  EXPECT_EQ(history.FindBool(day_key), true);
}

TEST_F(UniversalOptOutServiceTest, IneligibleWhenNotInTargetLocation) {
  EnableFeatureWithTargetLocations("us-tx");

  SetGeoLevel1("us-ny");
  auto service = CreateService();

  EXPECT_FALSE(service->IsEligible());
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUniversalOptOutEligible));

  const base::DictValue& history =
      pref_service_.GetDict(prefs::kUniversalOptOutEligibilityHistory);
  base::Time current_day = GetCurrentDay(test_clock_.Now());
  std::string day_key = base::TimeToValue(current_day).GetString();
  EXPECT_EQ(history.FindBool(day_key), false);
}

TEST_F(UniversalOptOutServiceTest, AveragingEligibleRatioAboveThreshold) {
  EnableFeatureWithTargetLocations("us-wa");

  // Populate history over the last 30 days: 16 eligible days, 14 ineligible.
  base::Time today = GetCurrentDay(test_clock_.Now());
  for (int i = 1; i <= 15; ++i) {
    SetHistoricalDay(today - base::Days(i), true);
  }
  for (int i = 16; i < 30; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }

  // Today is in target region (making 16 eligible days out of 30 total).
  SetGeoLevel1("us-wa");
  auto service = CreateService();

  EXPECT_TRUE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest, AveragingEligibleRatioBelowThreshold) {
  EnableFeatureWithTargetLocations("us-co");

  // Populate history over the last 30 days: 10 eligible days, 19 ineligible.
  base::Time today = GetCurrentDay(test_clock_.Now());
  for (int i = 1; i <= 10; ++i) {
    SetHistoricalDay(today - base::Days(i), true);
  }
  for (int i = 11; i < 30; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }

  // Today is outside target region (making 10 eligible days out of 30 total).
  SetGeoLevel1("us-il");
  auto service = CreateService();

  EXPECT_FALSE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest, SparseDataEvaluatesRatio) {
  EnableFeatureWithTargetLocations("us-va");

  // Sparse data: only 3 days recorded. 2 are eligible, 1 is ineligible (66% >=
  // 50%).
  base::Time today = GetCurrentDay(test_clock_.Now());
  SetHistoricalDay(today - base::Days(1), true);
  SetHistoricalDay(today - base::Days(5), false);

  SetGeoLevel1("us-va");
  auto service = CreateService();

  EXPECT_TRUE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest,
       BecomesIneligibleOutsideOfTrailingWindowWhenToggleOff) {
  EnableFeatureWithTargetLocations("us-or");

  // User is currently eligible.
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, false);

  // Populate last 90 days with mostly non-target days (e.g. 50 non-target, 10
  // target).
  base::Time today = GetCurrentDay(test_clock_.Now());
  for (int i = 1; i <= 10; ++i) {
    SetHistoricalDay(today - base::Days(i), true);
  }
  for (int i = 11; i < 60; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }

  SetGeoLevel1("us-az");
  auto service = CreateService();

  // User transitions to ineligible because non-target days > 50% over 90 days
  // and toggle is OFF.
  EXPECT_FALSE(service->IsEligible());
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kUniversalOptOutEligible));
}

TEST_F(UniversalOptOutServiceTest,
       RemainsEligibleWithinTrailingWindowWhenToggleOff) {
  EnableFeatureWithTargetLocations("us-or");

  // User is currently eligible and toggle is OFF.
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, false);

  // In the last 30 days, ratio < 50% (5 eligible, 25 ineligible).
  // In days 31-90, 55 eligible, 5 ineligible.
  // Overall in 90 days: 60 eligible out of 90 days (> 50%).
  base::Time today = GetCurrentDay(test_clock_.Now());
  for (int i = 1; i <= 5; ++i) {
    SetHistoricalDay(today - base::Days(i), true);
  }
  for (int i = 6; i <= 30; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }
  for (int i = 31; i <= 85; ++i) {
    SetHistoricalDay(today - base::Days(i), true);
  }
  for (int i = 86; i < 90; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }

  SetGeoLevel1("us-az");
  auto service = CreateService();

  // User remains eligible due to trailing window ratio >= 50%.
  EXPECT_TRUE(service->IsEligible());
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUniversalOptOutEligible));
}

TEST_F(UniversalOptOutServiceTest, RemainsEligibleWhenToggleOn) {
  EnableFeatureWithTargetLocations("us-nc");

  // User is currently eligible and has the toggle turned ON.
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, true);

  // Populate last 90 days with 100% non-target days.
  base::Time today = GetCurrentDay(test_clock_.Now());
  for (int i = 1; i < 90; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }

  SetGeoLevel1("us-ga");
  auto service = CreateService();

  // Because the toggle is enabled, the user does NOT become ineligible.
  EXPECT_TRUE(service->IsEligible());
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kUniversalOptOutEligible));
}

TEST_F(UniversalOptOutServiceTest, PrunesExpiredHistory) {
  EnableFeatureWithTargetLocations("us-ma");

  base::Time today = GetCurrentDay(test_clock_.Now());
  SetHistoricalDay(today - base::Days(5), true);
  SetHistoricalDay(today - base::Days(90),
                   true);  // Exactly at retention threshold -> pruned
  SetHistoricalDay(today - base::Days(120),
                   true);  // Older than 90 days -> pruned
  SetHistoricalDay(today + base::Days(5),
                   true);  // Future date (clock error) -> pruned

  SetGeoLevel1("us-ma");
  auto service = CreateService();

  const base::DictValue& history =
      pref_service_.GetDict(prefs::kUniversalOptOutEligibilityHistory);
  EXPECT_TRUE(history.contains(base::TimeToValue(today).GetString()));
  EXPECT_TRUE(
      history.contains(base::TimeToValue(today - base::Days(5)).GetString()));
  EXPECT_FALSE(
      history.contains(base::TimeToValue(today - base::Days(90)).GetString()));
  EXPECT_FALSE(
      history.contains(base::TimeToValue(today - base::Days(120)).GetString()));
  EXPECT_FALSE(
      history.contains(base::TimeToValue(today + base::Days(5)).GetString()));
}

TEST_F(UniversalOptOutServiceTest, OnSeedFetchedUpdatesEligibility) {
  EnableFeatureWithTargetLocations("us-hi");

  // Initially no location is set and user is not eligible.
  auto service = CreateService();
  EXPECT_FALSE(service->IsEligible());

  // A new seed arrives with target location.
  SetGeoLevel1("us-hi");
  service->OnSeedFetched();

  EXPECT_TRUE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest, MultipleTargetLocations) {
  EnableFeatureWithTargetLocations("us-ut,us-nv");

  SetGeoLevel1("us-ut");
  auto service = CreateService();
  EXPECT_TRUE(service->IsEligible());

  SetGeoLevel1("us-nv");
  test_clock_.Advance(base::Days(1));
  auto service2 = CreateService();
  EXPECT_TRUE(service2->IsEligible());

  SetGeoLevel1("us-oh");
  test_clock_.Advance(base::Days(1));
  auto service3 = CreateService();
  EXPECT_TRUE(service3->IsEligible());  // 2 out of 3 days eligible
}

TEST_F(UniversalOptOutServiceTest, SignedInUserEligibleWhenCapabilityIsTrue) {
  // When signed in with is_subject_to_universal_opt_out = true, the user is
  // eligible even if location history / pref indicates ineligibility.
  EnableFeatureWithTargetLocations("us-fl");
  SetGeoLevel1("us-ny");

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_universal_opt_out(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  auto service = CreateService();
  EXPECT_TRUE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest,
       SignedInUserIneligibleWhenCapabilityIsFalse) {
  // When signed in with is_subject_to_universal_opt_out = false, the user is
  // ineligible even if location history / pref indicates eligibility.
  EnableFeatureWithTargetLocations("us-fl");
  SetGeoLevel1("us-fl");

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_universal_opt_out(false);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  auto service = CreateService();
  EXPECT_FALSE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest,
       SignedInUserFallsBackToPrefEligibilityWhenCapabilityIsUnknown) {
  // Signed-in user with capability kUnknown falls back to pref eligibility
  // (which is true when in target location).
  EnableFeatureWithTargetLocations("us-fl");
  SetGeoLevel1("us-fl");

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  auto service = CreateService();
  EXPECT_TRUE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest,
       SignedInUserEligibilityUpdatesWhenCapabilityChanges) {
  EnableFeatureWithTargetLocations("us-fl");
  SetGeoLevel1("us-ny");

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_universal_opt_out(false);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  auto service = CreateService();
  EXPECT_FALSE(service->IsEligible());

  // Update capability to true.
  mutator.set_is_subject_to_universal_opt_out(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);
  EXPECT_TRUE(service->IsEligible());

#if !BUILDFLAG(IS_CHROMEOS)
  // User signs out; falls back to signed-out location eligibility (ineligible
  // here since us-ny != us-fl). ClearPrimaryAccount() is unsupported on
  // ChromeOS.
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_FALSE(service->IsEligible());
#endif
}

TEST_F(UniversalOptOutServiceTest, SignedOutUserFallsBackToPrefEligibility) {
  EnableFeatureWithTargetLocations("us-fl");
  SetGeoLevel1("us-fl");

  // User is not signed in.
  auto service = CreateService();
  EXPECT_TRUE(service->IsEligible());
}

TEST_F(UniversalOptOutServiceTest,
       StartupMetricsRecordedWhenSignedOutEligibleViaFinch) {
  EnableFeatureWithTargetLocations("us-ca");
  SetGeoLevel1("us-ca");

  base::HistogramTester histogram_tester;
  auto service = CreateService();

  histogram_tester.ExpectUniqueSample(kProfileEligibilityStartupHistogram, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kEligibilitySystemStartupHistogram,
                                      EligibilitySystem::kEligibleViaFinch, 1);
}

TEST_F(UniversalOptOutServiceTest,
       StartupMetricsRecordedWhenSignedOutIneligibleViaFinch) {
  EnableFeatureWithTargetLocations("us-ca");
  SetGeoLevel1("us-ny");

  base::HistogramTester histogram_tester;
  auto service = CreateService();

  histogram_tester.ExpectUniqueSample(kProfileEligibilityStartupHistogram,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(kEligibilitySystemStartupHistogram,
                                      EligibilitySystem::kIneligibleViaFinch,
                                      1);
}

TEST_F(UniversalOptOutServiceTest,
       StartupMetricsRecordedWhenSignedInEligibleViaAccountCapabilities) {
  EnableFeatureWithTargetLocations("us-ca");
  SetGeoLevel1("us-ny");  // Ineligible by Finch, but eligible by account.

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_universal_opt_out(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  base::HistogramTester histogram_tester;
  auto service = CreateService();

  histogram_tester.ExpectUniqueSample(kProfileEligibilityStartupHistogram, true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kEligibilitySystemStartupHistogram,
      EligibilitySystem::kEligibleViaAccountCapabilities, 1);
}

TEST_F(UniversalOptOutServiceTest,
       StartupMetricsRecordedWhenSignedInIneligibleViaAccountCapabilities) {
  EnableFeatureWithTargetLocations("us-ca");
  SetGeoLevel1("us-ca");  // Eligible by Finch, but ineligible by account.

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_universal_opt_out(false);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  base::HistogramTester histogram_tester;
  auto service = CreateService();

  histogram_tester.ExpectUniqueSample(kProfileEligibilityStartupHistogram,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      kEligibilitySystemStartupHistogram,
      EligibilitySystem::kIneligibleViaAccountCapabilities, 1);
}

TEST_F(UniversalOptOutServiceTest,
       StartupMetricsRecordedWhenSignedInCapabilityUnknownFallsBackToFinch) {
  EnableFeatureWithTargetLocations("us-ca");
  SetGeoLevel1("us-ca");

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;
  auto service = CreateService();

  histogram_tester.ExpectUniqueSample(kProfileEligibilityStartupHistogram, true,
                                      1);
  histogram_tester.ExpectUniqueSample(kEligibilitySystemStartupHistogram,
                                      EligibilitySystem::kEligibleViaFinch, 1);
}

TEST_F(UniversalOptOutServiceTest,
       EligibilityChangedHistogramRecordedOnTransition) {
  EnableFeatureWithTargetLocations("us-ca");
  SetGeoLevel1("us-ca");

  // User starts with pref false (default).
  base::HistogramTester histogram_tester;
  auto service = CreateService();

  histogram_tester.ExpectUniqueSample(
      kEligibilityChangedHistogram,
      EligibilityTransition::kIneligibleToEligible, 1);
}

TEST_F(UniversalOptOutServiceTest,
       EligibilityChangedHistogramRecordedOnLossOfEligibility) {
  EnableFeatureWithTargetLocations("us-ca");

  // User is eligible with toggle off.
  pref_service_.SetBoolean(prefs::kUniversalOptOutEligible, true);
  pref_service_.SetBoolean(prefs::kUniversalOptOutEnabled, false);

  // Populate history so ratio < 50% over 90 days.
  base::Time today = GetCurrentDay(test_clock_.Now());
  for (int i = 1; i <= 10; ++i) {
    SetHistoricalDay(today - base::Days(i), true);
  }
  for (int i = 11; i < 60; ++i) {
    SetHistoricalDay(today - base::Days(i), false);
  }

  SetGeoLevel1("us-ny");
  base::HistogramTester histogram_tester;
  auto service = CreateService();

  EXPECT_FALSE(service->IsEligible());
  histogram_tester.ExpectUniqueSample(
      kEligibilityChangedHistogram,
      EligibilityTransition::kEligibleToIneligible, 1);
}

}  // namespace universal_optout
