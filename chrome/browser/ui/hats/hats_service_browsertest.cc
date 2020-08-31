// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/hats_survey_status_checker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace {

base::test::ScopedFeatureList::FeatureAndParams probability_zero{
    features::kHappinessTrackingSurveysForDesktop,
    {{"probability", "0.000"}}};
base::test::ScopedFeatureList::FeatureAndParams probability_one{
    features::kHappinessTrackingSurveysForDesktop,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSatisfaction},
     {"en_site_id", "test_site_id"}}};
base::test::ScopedFeatureList::FeatureAndParams settings_probability_one{
    features::kHappinessTrackingSurveysForDesktopSettings,
    {{"probability", "1.000"},
     {"survey", kHatsSurveyTriggerSettings},
     {"en_site_id", "test_site_id"}}};

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetMetricsConsent);
};

class HatsSurveyStatusFakeChecker : public HatsSurveyStatusChecker {
 public:
  HatsSurveyStatusFakeChecker(Status status, base::OnceClosure quit_closure)
      : status_(status), quit_closure_(std::move(quit_closure)) {}
  ~HatsSurveyStatusFakeChecker() override = default;

  void CheckSurveyStatus(const std::string& site_id,
                         base::OnceClosure on_success,
                         base::OnceCallback<void(Status)> on_failure) override {
    if (status_ == Status::kSuccess) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::OnceClosure on_success, base::OnceClosure quit_closure) {
                std::move(on_success).Run();
                std::move(quit_closure).Run();
              },
              std::move(on_success), std::move(quit_closure_)));
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::OnceCallback<void(Status)> on_failure, Status status,
                 base::OnceClosure quit_closure) {
                std::move(on_failure).Run(status);
                std::move(quit_closure).Run();
              },
              std::move(on_failure), status_, std::move(quit_closure_)));
    }
  }

 protected:
  Status status_;
  base::OnceClosure quit_closure_;
};

class HatsServiceBrowserTestBase : public InProcessBrowserTest {
 protected:
  explicit HatsServiceBrowserTestBase(
      std::vector<base::test::ScopedFeatureList::FeatureAndParams>
          enabled_features)
      : enabled_features_(enabled_features) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_, {});
  }

  HatsServiceBrowserTestBase() = default;

  ~HatsServiceBrowserTestBase() override = default;

  void SetUpOnMainThread() override { SetTestSurveyChecker(); }

  HatsService* GetHatsService() {
    HatsService* service =
        HatsServiceFactory::GetForProfile(browser()->profile(), true);
    return service;
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  bool HatsBubbleShown() {
    views::BubbleDialogDelegateView* bubble = HatsBubbleView::GetHatsBubble();
    if (!bubble)
      return false;

    views::Widget* widget = bubble->GetWidget();
    return widget && widget->IsVisible();
  }

  void SetTestSurveyChecker(HatsSurveyStatusChecker::Status status =
                                HatsSurveyStatusChecker::Status::kSuccess) {
    run_loop_ = std::make_unique<base::RunLoop>();
    GetHatsService()->SetSurveyCheckerForTesting(
        std::make_unique<HatsSurveyStatusFakeChecker>(
            status, run_loop_->QuitClosure()));
  }

  void WaitForSurveyStatusCallback() { run_loop_->Run(); }

 private:
  base::Optional<ScopedSetMetricsConsent> scoped_metrics_consent_;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<base::test::ScopedFeatureList::FeatureAndParams>
      enabled_features_;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceBrowserTestBase);
};

class HatsServiceProbabilityZero : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityZero()
      : HatsServiceBrowserTestBase({probability_zero}) {}

  ~HatsServiceProbabilityZero() override = default;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityZero);
};

class HatsServiceProbabilityOne : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityOne()
      : HatsServiceBrowserTestBase(
            {probability_one, settings_probability_one}) {}

  ~HatsServiceProbabilityOne() override = default;

 private:
  void SetUpOnMainThread() override {
    HatsServiceBrowserTestBase::SetUpOnMainThread();

    // Set the profile creation time to be old enough to ensure triggering.
    browser()->profile()->SetCreationTimeForTesting(
        base::Time::Now() - base::TimeDelta::FromDays(45));
  }

  void TearDownOnMainThread() override {
    GetHatsService()->SetSurveyMetadataForTesting({});
  }

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityOne);
};

class HatsServiceImprovedCookieControlsEnabled
    : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceImprovedCookieControlsEnabled()
      : HatsServiceBrowserTestBase({probability_one}) {}

  ~HatsServiceImprovedCookieControlsEnabled() override = default;

 private:
  void SetUpOnMainThread() override {
    HatsServiceBrowserTestBase::SetUpOnMainThread();

    // Set the profile creation time to be old enough to ensure triggering.
    browser()->profile()->SetCreationTimeForTesting(
        base::Time::Now() - base::TimeDelta::FromDays(45));
  }

  void TearDownOnMainThread() override {
    GetHatsService()->SetSurveyMetadataForTesting({});
  }

  DISALLOW_COPY_AND_ASSIGN(HatsServiceImprovedCookieControlsEnabled);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceBrowserTestBase, BubbleNotShownOnDefault) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityZero, NoShow) {
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, NoShowConsentNotGiven) {
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, TriggerMismatchNoShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey("nonexistent-trigger");
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlwaysShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlsoShowsSettingsSurvey) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       DoubleShowOnlyResultsInOneShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());

  views::BubbleDialogDelegateView* bubble1 = HatsBubbleView::GetHatsBubble();
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_TRUE(HatsBubbleShown());
  EXPECT_EQ(bubble1, HatsBubbleView::GetHatsBubble());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SameMajorVersionNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = version_info::GetVersion().components()[0];
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DifferentMajorVersionShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = 42;
  ASSERT_NE(42u, version_info::GetVersion().components()[0]);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeRequiredElapsedTimeNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileTooYoungToShow) {
  SetMetricsConsent(true);
  // Set creation time to only 15 days.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() -
                                  base::TimeDelta::FromDays(15));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileOldEnoughToShow) {
  SetMetricsConsent(true);
  // Set creation time to 31 days. This is just past the threshold.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() -
                                  base::TimeDelta::FromDays(31));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, IncognitoModeDisabledNoShow) {
  SetMetricsConsent(true);
  // Disable incognito mode for this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kIncognitoModeAvailability,
                           IncognitoModePrefs::DISABLED);
  EXPECT_EQ(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(pref_service));

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedWithinADayNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_check_time =
      base::Time::Now() - base::TimeDelta::FromHours(23);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedAfterADayToShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_check_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyAlreadyFullNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.is_survey_full = true;
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyUnreachableNoShow) {
  SetMetricsConsent(true);
  // Make sure to start with clean metadata.
  HatsService::SurveyMetadata metadata;
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  ASSERT_FALSE(metadata.last_survey_check_time.has_value());

  HatsService* service = GetHatsService();
  SetTestSurveyChecker(HatsSurveyStatusChecker::Status::kUnreachable);
  service->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_FALSE(HatsBubbleShown());
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  EXPECT_TRUE(metadata.last_survey_check_time.has_value());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyFetchOverCapacityNoShow) {
  SetMetricsConsent(true);
  // Make sure to start with clean metadata.
  HatsService::SurveyMetadata metadata;
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  ASSERT_FALSE(metadata.is_survey_full.has_value());

  HatsService* service = GetHatsService();
  SetTestSurveyChecker(HatsSurveyStatusChecker::Status::kOverCapacity);
  service->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_FALSE(HatsBubbleShown());
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  EXPECT_TRUE(metadata.is_survey_full.has_value());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CookiesBlockedShow) {
  SetMetricsConsent(true);
  auto* settings_map =
      HostContentSettingsMapFactory::GetInstance()->GetForProfile(
          browser()->profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                         CONTENT_SETTING_BLOCK);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ThirdPartyCookiesBlockedShow) {
  SetMetricsConsent(true);
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, LaunchDelayedSurvey) {
  SetMetricsConsent(true);
  EXPECT_TRUE(
      GetHatsService()->LaunchDelayedSurvey(kHatsSurveyTriggerSatisfaction, 0));
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       LaunchDelayedSurveyForWebContents) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DisallowsEmptyWebContents) {
  SetMetricsConsent(true);
  EXPECT_FALSE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, nullptr, 0));
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(
    HatsServiceProbabilityOne,
    AllowsMultipleDelayedSurveyRequestsDifferentWebContents) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction,
      browser()->tab_strip_model()->GetActiveWebContents(), 0));
  WaitForSurveyStatusCallback();
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       DisallowsSameDelayedSurveyForWebContentsRequests) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  EXPECT_FALSE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  WaitForSurveyStatusCallback();
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ReleasesPendingTaskAfterFulfilling) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  WaitForSurveyStatusCallback();
  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       ReleasesPendingTaskAfterNoShow) {
  SetMetricsConsent(true);
  SetTestSurveyChecker(HatsSurveyStatusChecker::Status::kUnreachable);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0));
  WaitForSurveyStatusCallback();
  EXPECT_FALSE(GetHatsService()->HasPendingTasks());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, VisibleWebContentsShow) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, InvisibleWebContentsNoShow) {
  SetMetricsConsent(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerSatisfaction, web_contents, 0);
  chrome::AddTabAt(browser(), GURL(), -1, true);
  EXPECT_FALSE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceImprovedCookieControlsEnabled,
                       ThirdPartyCookiesBlockedInIncognitoShow) {
  SetMetricsConsent(true);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

IN_PROC_BROWSER_TEST_F(HatsServiceImprovedCookieControlsEnabled,
                       ThirdPartyCookiesAllowedInIncognitoShow) {
  SetMetricsConsent(true);
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSatisfaction);
  WaitForSurveyStatusCallback();
  EXPECT_TRUE(HatsBubbleShown());
}

class HatsServiceHatsNext : public HatsServiceProbabilityOne {
 public:
  HatsServiceHatsNext() {
    feature_list_.InitAndEnableFeature(
        features::kHappinessTrackingSurveysForDesktopMigration);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that once a HaTS Next dialog has been created, ShouldShowSurvey
// returns false until the service has been informed the dialog was closed.
IN_PROC_BROWSER_TEST_F(HatsServiceHatsNext, SingleHatsNextDialog) {
  SetMetricsConsent(true);
  EXPECT_TRUE(GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerTesting));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerTesting);

  // At this point a HaTS Next dialog is created and is attempting to contact
  // the wrapper website (which will fail as requests to non-localhost addresses
  // are disallowed in browser tests). Regardless of the outcome of the network
  // request, the dialog waits for a timeout posted to the UI thread before
  // closing itself. Since this test is also on the UI thread, these checks,
  // which rely on the dialog still being open, will not race.
  EXPECT_FALSE(GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerTesting));

  // Inform the service directly that the dialog has been closed.
  GetHatsService()->HatsNextDialogClosed();
  EXPECT_TRUE(GetHatsService()->ShouldShowSurvey(kHatsSurveyTriggerTesting));
}
