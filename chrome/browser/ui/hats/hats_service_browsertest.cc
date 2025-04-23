// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_hats_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/service/google_groups_manager_prefs.h"
#include "components/variations/variations_seed_processor.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {

constexpr char kRelevantGroupId[] = "1234";
constexpr base::TimeDelta kCooldownOverride = base::Days(14);

base::test::FeatureRefAndParams probability_zero{
    features::kHappinessTrackingSurveysForDesktopSettings,
    {{"probability", "0.000"}}};
base::test::FeatureRefAndParams probability_one{
    features::kHappinessTrackingSurveysForDesktopSettings,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSettings},
     {"en_site_id", "test_site_id"}}};
base::test::FeatureRefAndParams cool_down_period_overriden{
    autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate},
     {"cooldown-override-days", /*days=*/"14"}}};
base::test::FeatureRefAndParams cool_down_period_overriden_and_group_controlled{
    autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
    {{variations::internal::kGoogleGroupFeatureParamName, kRelevantGroupId},
     {"probability", "1.000"},
     {"survey", kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate},
     {"cooldown-override-days", /*days=*/"14"}}};

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ScopedSetMetricsConsent(const ScopedSetMetricsConsent&) = delete;
  ScopedSetMetricsConsent& operator=(const ScopedSetMetricsConsent&) = delete;

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;
};

class HatsServiceBrowserTestBase : public policy::PolicyTest {
 public:
  HatsServiceBrowserTestBase(const HatsServiceBrowserTestBase&) = delete;
  HatsServiceBrowserTestBase& operator=(const HatsServiceBrowserTestBase&) =
      delete;

 protected:
  explicit HatsServiceBrowserTestBase(
      std::vector<base::test::FeatureRefAndParams> enabled_features)
      : enabled_features_(enabled_features) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_, {});
  }

  HatsServiceBrowserTestBase() = default;
  ~HatsServiceBrowserTestBase() override = default;

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  HatsServiceDesktop* GetHatsService(Browser* browser = nullptr) {
    Profile* profile =
        browser ? browser->profile() : this->browser()->profile();
    HatsServiceDesktop* service = static_cast<HatsServiceDesktop*>(
        HatsServiceFactory::GetForProfile(profile, true));
    return service;
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  bool HatsNextDialogCreated(Browser* browser = nullptr) {
    return GetHatsService(browser)->hats_next_dialog_exists_for_testing();
  }

  // Mock a survey with a custom requested browser type. The `other_browser`
  // param may be used to mock the survey in another browser too. Returns the
  // trigger to use when launching the survey.
  std::string MockSurveyWithRequestedBrowserType(
      Browser* other_browser,
      hats::SurveyConfig::RequestedBrowserType requested_browser_type) {
    for (HatsServiceDesktop* service :
         {GetHatsService(), GetHatsService(other_browser)}) {
      service
          ->GetSurveyConfigsByTriggersForTesting()[kHatsSurveyTriggerSettings]
          .requested_browser_type = requested_browser_type;
    }
    return kHatsSurveyTriggerSettings;
  }

 private:
  std::optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<base::test::FeatureRefAndParams> enabled_features_;
};

class HatsServiceProbabilityZero : public HatsServiceBrowserTestBase {
 public:
  HatsServiceProbabilityZero(const HatsServiceProbabilityZero&) = delete;
  HatsServiceProbabilityZero& operator=(const HatsServiceProbabilityZero&) =
      delete;

 protected:
  HatsServiceProbabilityZero()
      : HatsServiceBrowserTestBase({probability_zero}) {}

  ~HatsServiceProbabilityZero() override = default;
};

class HatsServiceProbabilityOne : public HatsServiceBrowserTestBase {
 public:
  HatsServiceProbabilityOne(const HatsServiceProbabilityOne&) = delete;
  HatsServiceProbabilityOne& operator=(const HatsServiceProbabilityOne&) =
      delete;

 protected:
  HatsServiceProbabilityOne() : HatsServiceBrowserTestBase({probability_one}) {}

  ~HatsServiceProbabilityOne() override = default;

 private:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    // Set the profile creation time to be old enough to ensure triggering.
    browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                    base::Days(45));
  }

  void TearDownOnMainThread() override {
    GetHatsService()->SetSurveyMetadataForTesting({});
  }
};

class HatsServiceConfigCoolDownPeriodOverriden
    : public HatsServiceBrowserTestBase {
 public:
  HatsServiceConfigCoolDownPeriodOverriden(
      const HatsServiceConfigCoolDownPeriodOverriden&) = delete;
  HatsServiceConfigCoolDownPeriodOverriden& operator=(
      const HatsServiceConfigCoolDownPeriodOverriden&) = delete;

 protected:
  HatsServiceConfigCoolDownPeriodOverriden()
      : HatsServiceBrowserTestBase({cool_down_period_overriden}) {}

  ~HatsServiceConfigCoolDownPeriodOverriden() override = default;
};

class HatsServiceSurveyFeatureControlledByGroup
    : public HatsServiceBrowserTestBase {
 public:
  HatsServiceSurveyFeatureControlledByGroup(
      const HatsServiceSurveyFeatureControlledByGroup&) = delete;
  HatsServiceSurveyFeatureControlledByGroup& operator=(
      const HatsServiceSurveyFeatureControlledByGroup&) = delete;

 protected:
  HatsServiceSurveyFeatureControlledByGroup()
      : HatsServiceBrowserTestBase(
            {cool_down_period_overriden_and_group_controlled}) {}

  ~HatsServiceSurveyFeatureControlledByGroup() override = default;

  void AddProfileToGroup(const std::string& group) {
    base::Value::List pref_groups_list;
    base::Value::Dict group_dict;
    group_dict.Set(variations::kDogfoodGroupsSyncPrefGaiaIdKey, group);
    pref_groups_list.Append(std::move(group_dict));
    profile()->GetPrefs()->SetList(
#if BUILDFLAG(IS_CHROMEOS)
        variations::kOsDogfoodGroupsSyncPrefName,
#else
        variations::kDogfoodGroupsSyncPrefName,
#endif
        std::move(pref_groups_list));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceBrowserTestBase, BubbleNotShownOnDefault) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityZero, NoShow) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, NoShowConsentNotGiven) {
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());

  Browser* incognito_browser = CreateIncognitoBrowser();
  GetHatsService(incognito_browser)->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated(incognito_browser));
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, TriggerMismatchNoShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey("nonexistent-trigger");
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlwaysShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ShowWhenFeedbackSurveyPolicyEnabled) {
  SetMetricsConsent(true);
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kFeedbackSurveysEnabled, base::Value(true));
  UpdateProviderPolicy(policies);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       NoShowWhenFeedbackSurveyPolicyDisabled) {
  SetMetricsConsent(true);
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kFeedbackSurveysEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());

  Browser* incognito_browser = CreateIncognitoBrowser();
  auto trigger = MockSurveyWithRequestedBrowserType(
      incognito_browser, hats::SurveyConfig::RequestedBrowserType::kIncognito);
  GetHatsService(incognito_browser)->LaunchSurvey(trigger);
  EXPECT_FALSE(HatsNextDialogCreated(incognito_browser));
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceProbabilityOne,
    NeverShowWhenFeedbackSurveyPolicyEnabledWithoutMetricsConsent) {
  SetMetricsConsent(false);
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kFeedbackSurveysEnabled, base::Value(true));
  UpdateProviderPolicy(policies);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlsoShowsSettingsSurvey) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SameMajorVersionNoShow) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.last_major_version = version_info::GetVersion().components()[0];
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::
          kNoReceivedSurveyInCurrentMilestone,
      1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DifferentMajorVersionShow) {
  SetMetricsConsent(true);
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.last_major_version = 42;
  ASSERT_NE(42u, version_info::GetVersion().components()[0]);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeRequiredElapsedTimeNoShow) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoLastSurveyTooRecent, 1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeElapsedTimeBetweenAnySurveys) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.any_last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoAnyLastSurveyTooRecent,
      1);
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceConfigCoolDownPeriodOverriden,
    SurveyWithCoolddownOverride_FeatureNotGroupControlled_NoSurvey) {
  base::HistogramTester histogram_tester;
  SetMetricsConsent(true);
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(31));

  GoogleGroupsManager* groups_manager =
      GoogleGroupsManagerFactory::GetForBrowserContext(profile());
  EXPECT_FALSE(groups_manager->IsFeatureGroupControlled(
      autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey));
  EXPECT_TRUE(groups_manager->IsFeatureEnabledForProfile(
      autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey));
  // The cooldown override for the feature should be set to 14 days.
  EXPECT_EQ(base::FeatureParam<int>(
                &autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
                plus_addresses::hats::kCooldownOverrideDays, 0)
                .Get(),
            14);

  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.any_last_survey_started_time = base::Time::Now();
  metadata.any_last_survey_with_cooldown_override_started_time =
      base::Time::Now() - (kCooldownOverride + base::Days(1));
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  // kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate overrides the cool
  // down period.
  GetHatsService()->LaunchSurvey(
      kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate,
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(), /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      std::map<std::string, std::string>{
          {plus_addresses::hats::kPlusAddressesCount, "0"},
          {plus_addresses::hats::kFirstPlusAddressCreationTime, "0"},
          {plus_addresses::hats::kLastPlusAddressFillingTime, "0"}});
  EXPECT_FALSE(GetHatsService()->hats_next_dialog_exists_for_testing());
  // Since the feature is not group controlled, the cooldown override has no
  // effect. The default cooldown of 180 days is used, so the survey is on
  // cooldown.
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoAnyLastSurveyTooRecent,
      1);
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceSurveyFeatureControlledByGroup,
    SurveyWithCoolddownOverride_FeatureIsOnCooldown_NoSurvey) {
  base::HistogramTester histogram_tester;
  // Add the profile to the group assigned in the field trial config.
  // `GoogleGroupsManager::IsFeatureGroupControlled()` returns `false` without
  // this call.
  AddProfileToGroup(kRelevantGroupId);
  SetMetricsConsent(true);
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(31));

  GoogleGroupsManager* groups_manager =
      GoogleGroupsManagerFactory::GetForBrowserContext(profile());
  EXPECT_TRUE(groups_manager->IsFeatureGroupControlled(
      autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey));
  EXPECT_TRUE(groups_manager->IsFeatureEnabledForProfile(
      autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey));
  // The cooldown override for the feature should be set to 14 days.
  EXPECT_EQ(base::FeatureParam<int>(
                &autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
                plus_addresses::hats::kCooldownOverrideDays, 0)
                .Get(),
            14);

  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.any_last_survey_started_time = base::Time::Now() - base::Days(365);
  metadata.any_last_survey_with_cooldown_override_started_time =
      base::Time::Now() - (kCooldownOverride - base::Days(1));
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  // kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate overrides the cool
  // down period.
  GetHatsService()->LaunchSurvey(
      kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate,
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(), /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      std::map<std::string, std::string>{
          {plus_addresses::hats::kPlusAddressesCount, "0"},
          {plus_addresses::hats::kFirstPlusAddressCreationTime, "0"},
          {plus_addresses::hats::kLastPlusAddressFillingTime, "0"}});
  EXPECT_FALSE(GetHatsService()->hats_next_dialog_exists_for_testing());
  // Cooldown period is set, the feature is enabled for profile and group
  // controlled. However, the last survey with cooldown override was shown just
  // 2 days ago, so the survey is on cooldown.
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoAnyLastSurveyTooRecent,
      1);
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceSurveyFeatureControlledByGroup,
    SurveyWithCoolddownOverride_FeatureIsGroupControlled_StartsSurvey) {
  // Add the profile to the group assigned in the field trial config.
  // `GoogleGroupsManager::IsFeatureGroupControlled()` returns `false` without
  // this call.
  AddProfileToGroup(kRelevantGroupId);
  SetMetricsConsent(true);
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(31));

  GoogleGroupsManager* groups_manager =
      GoogleGroupsManagerFactory::GetForBrowserContext(profile());
  EXPECT_TRUE(groups_manager->IsFeatureGroupControlled(
      autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey));
  EXPECT_TRUE(groups_manager->IsFeatureEnabledForProfile(
      autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey));
  // The cooldown override for the feature should be set to 14 days.
  EXPECT_EQ(base::FeatureParam<int>(
                &autofill::features::kPlusAddressAcceptedFirstTimeCreateSurvey,
                plus_addresses::hats::kCooldownOverrideDays, 0)
                .Get(),
            14);

  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.any_last_survey_started_time = base::Time::Now();
  metadata.any_last_survey_with_cooldown_override_started_time =
      base::Time::Now() - (kCooldownOverride + base::Days(1));
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  // kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate overrides the cool
  // down period.
  GetHatsService()->LaunchSurvey(
      kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate,
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(), /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      std::map<std::string, std::string>{
          {plus_addresses::hats::kPlusAddressesCount, "0"},
          {plus_addresses::hats::kFirstPlusAddressCreationTime, "0"},
          {plus_addresses::hats::kLastPlusAddressFillingTime, "0"}});
  EXPECT_TRUE(GetHatsService()->hats_next_dialog_exists_for_testing());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileTooYoungToShow) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  // Set creation time to only 15 days.
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(15));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoProfileTooNew, 1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileOldEnoughToShow) {
  SetMetricsConsent(true);
  // Set creation time to 31 days. This is just past the threshold.
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Days(31));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       RegularSurveyInIncognitoNoShow) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;

  // A regular survey should not be shown in incognito
  Browser* incognito_browser = CreateIncognitoBrowser();
  GetHatsService(incognito_browser)->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoWrongBrowserType, 1);
  EXPECT_FALSE(HatsNextDialogCreated(incognito_browser));
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       IncognitoSurveyShownOnlyInIncognito) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;

  Browser* incognito_browser = CreateIncognitoBrowser();
  auto trigger = MockSurveyWithRequestedBrowserType(
      incognito_browser, hats::SurveyConfig::RequestedBrowserType::kIncognito);

  // An incognito survey should not be shown in regular
  GetHatsService()->LaunchSurvey(trigger);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoWrongBrowserType, 1);
  EXPECT_FALSE(HatsNextDialogCreated());

  // An incognito survey should be shown in incognito
  GetHatsService(incognito_browser)->LaunchSurvey(trigger);
  EXPECT_TRUE(HatsNextDialogCreated(incognito_browser));
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, IncognitoModeDisabledNoShow) {
  SetMetricsConsent(true);
  // Disable incognito mode for this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
  EXPECT_EQ(policy::IncognitoModeAvailability::kDisabled,
            IncognitoModePrefs::GetAvailability(pref_service));

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedWithinADayNoShow) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.last_survey_check_time = base::Time::Now() - base::Hours(23);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsServiceDesktop::ShouldShowSurveyReasons::kNoLastSurveyCheckTooRecent,
      1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedAfterADayToShow) {
  SetMetricsConsent(true);
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.last_survey_check_time = base::Time::Now() - base::Days(1);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyAlreadyFullNoShow) {
  SetMetricsConsent(true);
  HatsServiceDesktop::SurveyMetadata metadata;
  metadata.is_survey_full = true;
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, LaunchDelayedSurvey) {
  SetMetricsConsent(true);
  EXPECT_TRUE(
      GetHatsService()->LaunchDelayedSurvey(kHatsSurveyTriggerSettings, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       LaunchDelayedSurveyForWebContents) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DisallowsEmptyWebContents) {
  SetMetricsConsent(true);
  EXPECT_FALSE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, nullptr, 0));
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceProbabilityOne,
    AllowsMultipleDelayedSurveyRequestsDifferentWebContents) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings,
      browser()->tab_strip_model()->GetActiveWebContents(), 0));
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       DisallowsSameDelayedSurveyForWebContentsRequests) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0));
  EXPECT_FALSE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ReleasesPendingTaskAfterFulfilling) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, VisibleWebContentsShow) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, InvisibleWebContentsNoShow) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 0);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       NavigatedWebContents_RequireSameOrigin) {
  SetMetricsConsent(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/empty.html")));

  // As navigating also occurs asynchronously, a long survey delay is use to
  // ensure it completes before the survey tries to run.
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 10000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::REQUIRE_SAME_ORIGIN);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.test", "/empty.html")));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       NavigatedWebContents_NoRequireSameOrigin) {
  SetMetricsConsent(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/empty.html")));

  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 10000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::ALLOW_ANY);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.test", "/empty.html")));
  base::RunLoop().RunUntilIdle();

  // The survey task should still be in the pending task queue.
  EXPECT_TRUE(GetHatsService()->HasPendingTasks());
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       NavigatedWebContents_RequireSameDocument) {
  SetMetricsConsent(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL kTestUrl = embedded_test_server()->GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestUrl));

  // As navigating also occurs asynchronously, a long survey delay is use to
  // ensure it completes before the survey tries to run.
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 10000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::REQUIRE_SAME_DOCUMENT);

  // Same-document navigation
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.location='#';", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetHatsService()->HasPendingTasks());
  EXPECT_FALSE(HatsNextDialogCreated());

  // Same-origin navigation
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.test", "/empty_script.html")));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SameOriginNavigation) {
  SetMetricsConsent(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/empty.html")));

  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSettings, web_contents, 10000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::REQUIRE_SAME_ORIGIN);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/form.html")));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetHatsService()->HasPendingTasks());
  EXPECT_FALSE(HatsNextDialogCreated());
}

// Check that once a HaTS Next dialog has been created, ShouldShowSurvey
// returns false until the service has been informed the dialog was closed.
IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SingleHatsNextDialog) {
  SetMetricsConsent(true);
  EXPECT_TRUE(GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerSettings));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);

  // Clear any metadata that would prevent another survey from being displayed.
  GetHatsService()->SetSurveyMetadataForTesting({});

  // At this point a HaTS Next dialog is created and is attempting to contact
  // the wrapper website (which will fail as requests to non-localhost addresses
  // are disallowed in browser tests). Regardless of the outcome of the network
  // request, the dialog waits for a timeout posted to the UI thread before
  // closing itself. Since this test is also on the UI thread, these checks,
  // which rely on the dialog still being open, will not race.
  EXPECT_FALSE(GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerSettings));

  // Inform the service directly that the dialog has been closed.
  GetHatsService()->HatsNextDialogClosed();
  EXPECT_TRUE(GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerSettings));
}

// Check that launching a HaTS Next survey records a survey check time
IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyCheckTimeRecorded) {
  SetMetricsConsent(true);

  // Clear any existing survey metadata.
  GetHatsService()->SetSurveyMetadataForTesting({});

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);

  HatsServiceDesktop::SurveyMetadata metadata;
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  EXPECT_TRUE(metadata.last_survey_check_time.has_value());
}

// Check that launching a HaTS Next survey records a survey check time even if
// triggered in incognito
IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyCheckTimeRecordedIncognito) {
  SetMetricsConsent(true);

  // Clear any existing survey metadata.
  GetHatsService()->SetSurveyMetadataForTesting({});

  Browser* incognito_browser = CreateIncognitoBrowser();
  auto trigger = MockSurveyWithRequestedBrowserType(
      incognito_browser, hats::SurveyConfig::RequestedBrowserType::kIncognito);

  GetHatsService(incognito_browser)->LaunchSurvey(trigger);

  HatsServiceDesktop::SurveyMetadata metadata;
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  EXPECT_TRUE(metadata.last_survey_check_time.has_value());
}
