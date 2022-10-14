// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

base::test::FeatureRefAndParams probability_zero{
    features::kHappinessTrackingSurveysForDesktopSettings,
    {{"probability", "0.000"}}};
base::test::FeatureRefAndParams probability_one{
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

  ScopedSetMetricsConsent(const ScopedSetMetricsConsent&) = delete;
  ScopedSetMetricsConsent& operator=(const ScopedSetMetricsConsent&) = delete;

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;
};

class HatsServiceBrowserTestBase : public InProcessBrowserTest {
 protected:
  explicit HatsServiceBrowserTestBase(
      std::vector<base::test::FeatureRefAndParams> enabled_features)
      : enabled_features_(enabled_features) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_, {});
  }

  HatsServiceBrowserTestBase() = default;

  HatsServiceBrowserTestBase(const HatsServiceBrowserTestBase&) = delete;
  HatsServiceBrowserTestBase& operator=(const HatsServiceBrowserTestBase&) =
      delete;

  ~HatsServiceBrowserTestBase() override = default;

  HatsService* GetHatsService() {
    HatsService* service =
        HatsServiceFactory::GetForProfile(browser()->profile(), true);
    return service;
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  bool HatsNextDialogCreated() {
    return GetHatsService()->hats_next_dialog_exists_for_testing();
  }

 private:
  absl::optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

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
  HatsService::SurveyMetadata metadata;
  metadata.last_major_version = version_info::GetVersion().components()[0];
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoReceivedSurveyInCurrentMilestone,
      1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, DifferentMajorVersionShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
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
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoLastSurveyTooRecent, 1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne,
                       SurveyStartedBeforeElapsedTimeBetweenAnySurveys) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  HatsService::SurveyMetadata metadata;
  metadata.any_last_survey_started_time = base::Time::Now();
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoAnyLastSurveyTooRecent, 1);
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileTooYoungToShow) {
  SetMetricsConsent(true);
  base::HistogramTester histogram_tester;
  // Set creation time to only 15 days.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() - base::Days(15));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  histogram_tester.ExpectUniqueSample(
      kHatsShouldShowSurveyReasonHistogram,
      HatsService::ShouldShowSurveyReasons::kNoProfileTooNew, 1);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, ProfileOldEnoughToShow) {
  SetMetricsConsent(true);
  // Set creation time to 31 days. This is just past the threshold.
  static_cast<ProfileImpl*>(browser()->profile())
      ->SetCreationTimeForTesting(base::Time::Now() - base::Days(31));
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, IncognitoModeDisabledNoShow) {
  SetMetricsConsent(true);
  // Disable incognito mode for this profile.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(
      prefs::kIncognitoModeAvailability,
      static_cast<int>(IncognitoModePrefs::Availability::kDisabled));
  EXPECT_EQ(IncognitoModePrefs::Availability::kDisabled,
            IncognitoModePrefs::GetAvailability(pref_service));

  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedWithinADayNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_check_time = base::Time::Now() - base::Hours(23);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_FALSE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, CheckedAfterADayToShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
  metadata.last_survey_check_time = base::Time::Now() - base::Days(1);
  GetHatsService()->SetSurveyMetadataForTesting(metadata);
  GetHatsService()->LaunchSurvey(kHatsSurveyTriggerSettings);
  EXPECT_TRUE(HatsNextDialogCreated());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, SurveyAlreadyFullNoShow) {
  SetMetricsConsent(true);
  HatsService::SurveyMetadata metadata;
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
      /*require_same_origin=*/true);
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
      /*require_same_origin=*/false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.test", "/empty.html")));
  base::RunLoop().RunUntilIdle();

  // The survey task should still be in the pending task queue.
  EXPECT_TRUE(GetHatsService()->HasPendingTasks());
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
      /*require_same_origin=*/true);
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

  HatsService::SurveyMetadata metadata;
  GetHatsService()->GetSurveyMetadataForTesting(&metadata);
  EXPECT_TRUE(metadata.last_survey_check_time.has_value());
}
