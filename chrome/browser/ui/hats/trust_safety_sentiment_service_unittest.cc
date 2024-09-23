// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

class TrustSafetySentimentServiceTest : public testing::Test {
 public:
  TrustSafetySentimentServiceTest() {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

  void SetUp() override {
    metrics::DesktopSessionDurationTracker::Initialize();
  }

  void TearDown() override {
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

 protected:
  struct FeatureParams {
    std::string privacy_settings_time = "20s";
    std::string min_time_to_prompt = "2m";
    std::string max_time_to_prompt = "60m";
    std::string ntp_visits_min_range = "2";
    std::string ntp_visits_max_range = "4";
    std::string privacy_settings_probability = "0.6";
    std::string trusted_surface_probability = "0.4";
    std::string transactions_probability = "0.05";
    std::string privacy_sandbox_4_consent_accept_probability = "0.01";
    std::string privacy_sandbox_4_consent_decline_probability = "0.1";
    std::string privacy_sandbox_4_notice_ok_probability = "0.1";
    std::string privacy_sandbox_4_notice_settings_probability = "0.1";
    std::string privacy_settings_trigger_id = "privacy-settings-test";
    std::string trusted_surface_trigger_id = "trusted-surface-test";
    std::string transactions_trigger_id = "transactions-test";
    std::string privacy_sandbox_4_consent_accept_trigger_id =
        "privacy-sandbox-4-consent-accept";
    std::string privacy_sandbox_4_consent_decline_trigger_id =
        "privacy-sandbox-4-consent-decline";
    std::string privacy_sandbox_4_notice_ok_trigger_id =
        "privacy-sandbox-4-notice-ok";
    std::string privacy_sandbox_4_notice_settings_trigger_id =
        "privacy-sandbox-4-notice-settings";
    std::string transactions_password_manager_time = "20s";
  };

  void SetupFeatureParameters(FeatureParams params) {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kTrustSafetySentimentSurvey,
        {
            {"privacy-settings-time", params.privacy_settings_time},
            {"min-time-to-prompt", params.min_time_to_prompt},
            {"max-time-to-prompt", params.max_time_to_prompt},
            {"ntp-visits-min-range", params.ntp_visits_min_range},
            {"ntp-visits-max-range", params.ntp_visits_max_range},
            {"privacy-settings-probability",
             params.privacy_settings_probability},
            {"trusted-surface-probability", params.trusted_surface_probability},
            {"transactions-probability", params.transactions_probability},
            {"privacy-sandbox-4-consent-accept-probability",
             params.privacy_sandbox_4_consent_accept_probability},
            {"privacy-sandbox-4-consent-decline-probability",
             params.privacy_sandbox_4_consent_decline_probability},
            {"privacy-sandbox-4-notice-ok-probability",
             params.privacy_sandbox_4_notice_ok_probability},
            {"privacy-sandbox-4-notice-settings-probability",
             params.privacy_sandbox_4_notice_settings_probability},
            {"privacy-settings-trigger-id", params.privacy_settings_trigger_id},
            {"trusted-surface-trigger-id", params.trusted_surface_trigger_id},
            {"transactions-trigger-id", params.transactions_trigger_id},
            {"privacy-sandbox-4-consent-accept-trigger-id",
             params.privacy_sandbox_4_consent_accept_trigger_id},
            {"privacy-sandbox-4-consent-decline-trigger-id",
             params.privacy_sandbox_4_consent_decline_trigger_id},
            {"privacy-sandbox-4-notice-ok-trigger-id",
             params.privacy_sandbox_4_notice_ok_trigger_id},
            {"privacy-sandbox-4-notice-settings-trigger-id",
             params.privacy_sandbox_4_notice_settings_trigger_id},
            {"transactions-password-manager-time",
             params.transactions_password_manager_time},
        });
  }

  struct FeatureParamsV2 {
    std::string min_time_to_prompt = "2m";
    std::string max_time_to_prompt = "60m";
    std::string ntp_visits_min_range = "2";
    std::string ntp_visits_max_range = "4";
    std::string min_session_time = "30s";
    std::string trusted_surface_time = "5s";
    std::string browsing_data_probability = "0.4";
    std::string control_group_probability = "0.4";
    std::string download_warning_ui_probability = "0.0";
    std::string password_check_probability = "0.4";
    std::string password_protection_ui_probability = "0.0";
    std::string safety_check_probability = "0.4";
    std::string safety_hub_notification_probability = "0.4";
    std::string safety_hub_interaction_probability = "0.4";
    std::string trusted_surface_probability = "0.4";
    std::string privacy_guide_probability = "0.4";
    std::string privacy_sandbox_4_consent_accept_probability = "0.01";
    std::string privacy_sandbox_4_consent_decline_probability = "0.1";
    std::string privacy_sandbox_4_notice_ok_probability = "0.1";
    std::string privacy_sandbox_4_notice_settings_probability = "0.1";
    std::string safe_browsing_interstitial_probability = "0.4";
    std::string browsing_data_trigger_id = "browsing-data-test";
    std::string control_group_trigger_id = "control-group-test";
    std::string download_warning_ui_trigger_id = "download-warning-ui-test";
    std::string password_check_trigger_id = "password-check-test";
    std::string password_protection_ui_trigger_id =
        "password-protection-ui-test";
    std::string safety_check_trigger_id = "safety-check-test";
    std::string trusted_surface_trigger_id = "trusted-surface-test";
    std::string privacy_guide_trigger_id = "privacy-guide-test";
    std::string privacy_sandbox_4_consent_accept_trigger_id =
        "privacy-sandbox-4-consent-accept";
    std::string privacy_sandbox_4_consent_decline_trigger_id =
        "privacy-sandbox-4-consent-decline";
    std::string privacy_sandbox_4_notice_ok_trigger_id =
        "privacy-sandbox-4-notice-ok";
    std::string privacy_sandbox_4_notice_settings_trigger_id =
        "privacy-sandbox-4-notice-settings";
    std::string safe_browsing_interstitial_trigger_id =
        "safe-browsing-interstitial";
  };

  void SetupFeatureParametersV2(FeatureParamsV2 params) {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kTrustSafetySentimentSurveyV2,
        {
            {"min-time-to-prompt", params.min_time_to_prompt},
            {"max-time-to-prompt", params.max_time_to_prompt},
            {"ntp-visits-min-range", params.ntp_visits_min_range},
            {"ntp-visits-max-range", params.ntp_visits_max_range},
            {"min-session-time", params.min_session_time},
            {"trusted-surface-time", params.trusted_surface_time},
            {"browsing-data-probability", params.browsing_data_probability},
            {"control-group-probability", params.control_group_probability},
            {"download-warning-ui-probability",
             params.download_warning_ui_probability},
            {"password-check-probability", params.password_check_probability},
            {"password-protection-ui-probability",
             params.password_protection_ui_probability},
            {"safety-check-probability", params.safety_check_probability},
            {"safety-hub-notification-probability",
             params.safety_hub_notification_probability},
            {"safety-hub-interaction-probability",
             params.safety_hub_interaction_probability},
            {"trusted-surface-probability", params.trusted_surface_probability},
            {"privacy-guide-probability", params.privacy_guide_probability},
            {"privacy-sandbox-4-consent-accept-probability",
             params.privacy_sandbox_4_consent_accept_probability},
            {"privacy-sandbox-4-consent-decline-probability",
             params.privacy_sandbox_4_consent_decline_probability},
            {"privacy-sandbox-4-notice-ok-probability",
             params.privacy_sandbox_4_notice_ok_probability},
            {"privacy-sandbox-4-notice-settings-probability",
             params.privacy_sandbox_4_notice_settings_probability},
            {"safe-browsing-interstitial-probability",
             params.safe_browsing_interstitial_probability},
            {"browsing-data-trigger-id", params.browsing_data_trigger_id},
            {"control-group-trigger-id", params.control_group_trigger_id},
            {"download-warning-ui-trigger-id",
             params.download_warning_ui_trigger_id},
            {"password-check-trigger-id", params.password_check_trigger_id},
            {"password-protection-ui-trigger-id",
             params.password_protection_ui_trigger_id},
            {"safety-check-trigger-id", params.safety_check_trigger_id},
            {"trusted-surface-trigger-id", params.trusted_surface_trigger_id},
            {"privacy-guide-trigger-id", params.privacy_guide_trigger_id},
            {"privacy-sandbox-4-consent-accept-trigger-id",
             params.privacy_sandbox_4_consent_accept_trigger_id},
            {"privacy-sandbox-4-consent-decline-trigger-id",
             params.privacy_sandbox_4_consent_decline_trigger_id},
            {"privacy-sandbox-4-notice-ok-trigger-id",
             params.privacy_sandbox_4_notice_ok_trigger_id},
            {"privacy-sandbox-4-notice-settings-trigger-id",
             params.privacy_sandbox_4_notice_settings_trigger_id},
            {"safe-browsing-interstitial-trigger-id",
             params.safe_browsing_interstitial_trigger_id},
        });
  }

  void CheckHistograms(
      const std::set<TrustSafetySentimentService::FeatureArea>& triggered_areas,
      const std::set<TrustSafetySentimentService::FeatureArea>&
          surveyed_areas) {
    std::map<std::string, std::set<TrustSafetySentimentService::FeatureArea>>
        histogram_to_expected = {
            {"Feedback.TrustSafetySentiment.TriggerOccurred", triggered_areas},
            {"Feedback.TrustSafetySentiment.SurveyRequested", surveyed_areas}};

    for (const auto& histogram_expected : histogram_to_expected) {
      const auto& histogram_name = histogram_expected.first;
      const auto& expected = histogram_expected.second;
      histogram_tester()->ExpectTotalCount(histogram_name, expected.size());
      for (auto area : expected) {
        histogram_tester()->ExpectBucketCount(histogram_name, area, 1);
      }
    }
  }

  void CheckCallTriggerOccurredHistogram(
      const std::map<TrustSafetySentimentService::FeatureArea, int>&
          triggered_area_counts) {
    int total_count = 0;
    const std::string histogram_name =
        "Feedback.TrustSafetySentiment.CallTriggerOccurred";
    for (const auto& histogram_expected : triggered_area_counts) {
      const auto& feature_area = histogram_expected.first;
      int expected_count = histogram_expected.second;
      histogram_tester()->ExpectBucketCount(histogram_name, feature_area,
                                            expected_count);
      total_count += expected_count;
    }
    histogram_tester()->ExpectTotalCount(histogram_name, total_count);
  }

  TrustSafetySentimentService* service() {
    return TrustSafetySentimentServiceFactory::GetForProfile(profile());
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

TEST_F(TrustSafetySentimentServiceTest, Eligibility_NtpOpens) {
  // A survey should not be shown if not enough NTPs have been opened since
  // the most recent trigger action.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "2";
  params.ntp_visits_max_range = "2";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  // The Trusted Surface trigger should prevent a survey from being shown, as
  // it still has 1 NTP to open.
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPrivacySettings, 1},
       {TrustSafetySentimentService::FeatureArea::kTrustedSurface, 1}});

  // The next NTP should be eligible for a survey.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _));
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, Eligibility_Time) {
  // A survey should not be shown if any trigger action occurred to recently, or
  // if all trigger actions occurred too long ago.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "1m";
  params.max_time_to_prompt = "10m";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();

  task_environment()->AdvanceClock(base::Minutes(2));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Moving the clock forward such that only the trusted surface trigger is
  // within the window should guarantee it is the survey shown.
  task_environment()->AdvanceClock(base::Minutes(9));
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface, _, _, _, _));
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {TrustSafetySentimentService::FeatureArea::kTrustedSurface});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPrivacySettings, 1},
       {TrustSafetySentimentService::FeatureArea::kTrustedSurface, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, TriggerProbability) {
  // Triggers which fail the probability check should not be considered.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  FeatureParams params;
  params.trusted_surface_probability = "0.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  SetupFeatureParameters(params);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
  CheckHistograms({}, {});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kTrustedSurface, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, TriggersClearOnLaunch) {
  // Check that all active triggers are cleared when a survey is launched.
  FeatureParams params;
  params.trusted_surface_probability = "1.0";
  params.privacy_settings_probability = "1.0";
  params.transactions_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {});

  // The launched survey will be randomly selected from the two triggers.
  std::string requested_survey_trigger;
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _))
      .WillOnce(testing::SaveArg<0>(&requested_survey_trigger));
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  auto surveyed_feature_area =
      requested_survey_trigger == kHatsSurveyTriggerTrustSafetyPrivacySettings
          ? TrustSafetySentimentService::FeatureArea::kPrivacySettings
          : TrustSafetySentimentService::FeatureArea::kTrustedSurface;

  // The trigger which did not result in a survey should no longer be
  // considered.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Repeated triggers post survey launch should however be considered.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _, _, _));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTransactions, {});
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface,
                   TrustSafetySentimentService::FeatureArea::kTransactions},
                  {surveyed_feature_area,
                   TrustSafetySentimentService::FeatureArea::kTransactions});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPrivacySettings, 1},
       {TrustSafetySentimentService::FeatureArea::kTrustedSurface, 1},
       {TrustSafetySentimentService::FeatureArea::kTransactions, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, SettingsWatcher_PrivacySettings) {
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.privacy_settings_time = "10s";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  // Create and navigate a test web contents to settings.
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUISettingsURL));

  // Interacting with setting shouldn't causes a survey to be immediately
  // displayed, but should require the user to stay on settings for some time.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->InteractedWithPrivacySettings(web_contents.get());
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Once the user has spent the appropriate amount of time on settings, they
  // should be eligible for a survey.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  task_environment()->AdvanceClock(base::Seconds(20));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Leaving settings before the required time should disqualify the user from
  // receiving a survey.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->AdvanceClock(base::Seconds(5));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("http://unrelated.com"));
  task_environment()->AdvanceClock(base::Seconds(15));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, SettingsWatcher_PasswordManager) {
  FeatureParams params;
  params.transactions_probability = "1.0";
  params.transactions_password_manager_time = "10s";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  // Check that after being informed of a visit to the password manager page,
  // the service correctly watches the provided WebContents to check if the
  // user stays on settings.
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUISettingsURL));

  // A survey should not be shown unless the user spends at least the required
  // time on settings after opening password manager.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->OpenedPasswordManager(web_contents.get());
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Once the user has spent sufficient time on settings after visiting the
  // password manager, they should be eligible for the Transactions copy of the
  // survey.
  SurveyBitsData expected_psd = {{"Saved password", false}};
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _,
                           expected_psd, _));

  task_environment()->AdvanceClock(base::Seconds(20));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Leaving settings before the required time should not make the user
  // eligible.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->OpenedPasswordManager(web_contents.get());
  task_environment()->AdvanceClock(base::Seconds(5));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("http://unrelated.com"));
  task_environment()->AdvanceClock(base::Seconds(15));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, RanSafetyCheck) {
  // Running the safety check is considered a trigger, and should make a user
  // eligible to receive a survey.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, SavedPassword) {
  // Saving a password is considered a trigger, and should make a user eligible
  // to receive a survey.
  FeatureParams params;
  params.transactions_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  SurveyBitsData expected_psd = {{"Saved password", true}};

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _,
                           expected_psd, _));
  service()->SavedPassword();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, SavedCard) {
  // Saving a card is considered a trigger, and should make a user eligible
  // to receive a survey.
  FeatureParams params;
  params.transactions_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  SurveyBitsData expected_psd = {{"Saved password", false}};

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _,
                           expected_psd, _));
  service()->SavedCard();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest,
       InteractedWithPrivacySandbox4ConsentAccept) {
  // Accepting Privacy Sandbox 4 consent is considered a trigger, and should
  // make a user eligible to receive a survey.
  FeatureParams params;
  params.privacy_sandbox_4_consent_accept_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentAccept, _,
                   _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4ConsentAccept);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest,
       InteractedWithPrivacySandbox4ConsentDecline) {
  // Declining Privacy Sandbox 4 consent is considered a trigger, and should
  // make a user eligible to receive a survey.
  FeatureParams params;
  params.privacy_sandbox_4_consent_decline_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySandbox4ConsentDecline,
                   _, _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4ConsentDecline);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, InteractedWithPrivacySandbox4NoticeOk) {
  // Acknowledging the Privacy Sandbox 4 notice is considered a trigger, and
  // should make a user eligible to receive a survey.
  FeatureParams params;
  params.privacy_sandbox_4_notice_ok_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeOk,
                           _, _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeOk);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest,
       InteractedWithPrivacySandbox4NoticeSettings) {
  // Going to settings from the Privacy Sandbox 4 notice is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParams params;
  params.privacy_sandbox_4_notice_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySandbox4NoticeSettings,
                   _, _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeSettings);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, PrivacySettingsProductSpecificData) {
  // Check the product specific data accompanying surveys for the Privacy
  // Settings feature area correctly records whether the user has a non default
  // privacy setting.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.privacy_settings_time = "0s";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUISettingsURL));

  SurveyBitsData expected_psd = {{"Non default setting", false},
                                 {"Ran safety check", false}};

  // By default, a user should have no non-default settings.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  expected_psd["Ran safety check"] = true;
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Check that default content settings are considered.
  expected_psd["Non default setting"] = true;
  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetDefaultContentSetting(
      ContentSettingsType::SOUND, ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  content_settings->SetDefaultContentSetting(
      ContentSettingsType::SOUND, ContentSetting::CONTENT_SETTING_DEFAULT);

  // Check that preferences are considered.
  expected_psd["Ran safety check"] = false;
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kEnableDoNotTrack, std::make_unique<base::Value>(true));
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  profile()->GetPrefs()->ClearPref(prefs::kEnableDoNotTrack);

  // Check that sync state defaults are handled correctly.
  profile()->GetTestingPrefService()->SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // UKM is only non default while no sync consent is present.
  expected_psd["Non default setting"] = false;
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kGoogleServicesConsentedToSync,
      std::make_unique<base::Value>(true));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  // A preference or content setting changed via policy should not be considered
  // as non-default.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEnableDoNotTrack, std::make_unique<base::Value>(true));
  auto managed_provider = std::make_unique<content_settings::MockProvider>();
  managed_provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES,
      base::Value(ContentSetting::CONTENT_SETTING_BLOCK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content_settings::TestUtils::OverrideProvider(
      content_settings, std::move(managed_provider),
      content_settings::ProviderType::kPolicyProvider);
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, ActiveIncognitoPreventsSurvey) {
  // Check that when an incognito profile exists that a survey is not shown.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  auto* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  profile()->DestroyOffTheRecordProfile(otr_profile);
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kIneligible},
                  {TrustSafetySentimentService::FeatureArea::kPrivacySettings});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kSafetyCheck, 1},
       {TrustSafetySentimentService::FeatureArea::kPrivacySettings, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, ClosingIncognitoDelaysSurvey) {
  // Check that closing an incognito session delays when a survey is shown
  // by the minimum time to prompt, and the maximum of the range of NTP visits.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "1m";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "2";
  SetupFeatureParameters(params);

  auto* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->RanSafetyCheck();

  // Record 2 visits to the NTP so regardless of the random NTP count chosen,
  // the Privacy Settings trigger will be eligible, but currently blocked by
  // the presence of an incognito profile.
  for (int i = 0; i < 2; i++)
    service()->OpenedNewTabPage();

  profile()->DestroyOffTheRecordProfile(otr_profile);

  // The first NTP opened after closing the incognito session should never
  // result in a survey, as the maximum of the range is 2.
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kIneligible},
                  {});

  // The second visit to the NTP should not trigger a survey if it takes place
  // less than the minimum time to prompt after closing an incognito session.
  task_environment()->AdvanceClock(base::Seconds(30));
  service()->OpenedNewTabPage();

  // Up to this point no attempt to show any survey should have been made.
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));

  // The next tab open which occurs after the required number of opens, and the
  // minimum time has passed, should trigger a survey.
  task_environment()->AdvanceClock(base::Minutes(1));
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kIneligible},
                  {TrustSafetySentimentService::FeatureArea::kPrivacySettings});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kSafetyCheck, 1},
       {TrustSafetySentimentService::FeatureArea::kPrivacySettings, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, AllFeatureAreasHaveTriggers) {
  // Assert that for every feature area there is the correct version(s) and
  // survey trigger id.
  FeatureParams paramsv1;
  SetupFeatureParameters(paramsv1);
  for (int enum_value = 0;
       enum_value <=
       static_cast<int>(TrustSafetySentimentService::FeatureArea::kMaxValue);
       ++enum_value) {
    // Skip deprecated PrivacySandbox3 values.
    if (enum_value >= 4 && enum_value <= 9) {
      continue;
    }
    auto feature_area =
        static_cast<TrustSafetySentimentService::FeatureArea>(enum_value);
    if (TrustSafetySentimentService::VersionCheck(feature_area)) {
      EXPECT_NE("", TrustSafetySentimentService::GetHatsTriggerForFeatureArea(
                        feature_area));
    }
  }
}

TEST_F(TrustSafetySentimentServiceTest, V2_AllFeatureAreasHaveTriggers) {
  // Assert that for every feature area there is the correct version(s) and
  // survey trigger id.
  FeatureParamsV2 paramsv2;
  SetupFeatureParametersV2(paramsv2);
  for (int enum_value = 0;
       enum_value <=
       static_cast<int>(TrustSafetySentimentService::FeatureArea::kMaxValue);
       ++enum_value) {
    // Skip deprecated PrivacySandbox3 values.
    if (enum_value >= 4 && enum_value <= 9) {
      continue;
    }
    auto feature_area =
        static_cast<TrustSafetySentimentService::FeatureArea>(enum_value);
    if (TrustSafetySentimentService::VersionCheck(feature_area)) {
      EXPECT_NE("", TrustSafetySentimentService::GetHatsTriggerForFeatureArea(
                        feature_area));
    }
  }
}

TEST_F(TrustSafetySentimentServiceTest, AllFeatureAreasHaveProbabilities) {
  // Check that for every feature with a probability of 1 and the correct
  // version, the dice roll always succeeds.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.transactions_probability = "1.0";
  params.privacy_sandbox_4_consent_accept_probability = "1.0";
  params.privacy_sandbox_4_consent_decline_probability = "1.0";
  params.privacy_sandbox_4_notice_ok_probability = "1.0";
  params.privacy_sandbox_4_notice_settings_probability = "1.0";

  SetupFeatureParameters(params);
  for (int enum_value = 0;
       enum_value <=
       static_cast<int>(TrustSafetySentimentService::FeatureArea::kMaxValue);
       ++enum_value) {
    // Skip deprecated PrivacySandbox3 values.
    if (enum_value >= 4 && enum_value <= 9) {
      continue;
    }
    auto feature_area =
        static_cast<TrustSafetySentimentService::FeatureArea>(enum_value);
    if (TrustSafetySentimentService::VersionCheck(feature_area)) {
      EXPECT_TRUE(TrustSafetySentimentService::ProbabilityCheck(feature_area))
          << "Feature area: " << static_cast<int>(feature_area);
    }
  }
}

TEST_F(TrustSafetySentimentServiceTest, V2_AllFeatureAreasHaveProbabilities) {
  // Check that for every feature with a probability of 1 and the correct
  // version, the dice roll always succeeds.
  FeatureParamsV2 params;
  params.browsing_data_probability = "1.0";
  params.control_group_probability = "1.0";
  params.download_warning_ui_probability = "1.0";
  params.password_check_probability = "1.0";
  params.password_protection_ui_probability = "1.0";
  params.safety_check_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.privacy_guide_probability = "1.0";
  params.privacy_sandbox_4_consent_accept_probability = "1.0";
  params.privacy_sandbox_4_consent_decline_probability = "1.0";
  params.privacy_sandbox_4_notice_ok_probability = "1.0";
  params.privacy_sandbox_4_notice_settings_probability = "1.0";
  params.safe_browsing_interstitial_probability = "1.0";
  params.safety_hub_notification_probability = "1.0";
  params.safety_hub_interaction_probability = "1.0";

  SetupFeatureParametersV2(params);
  for (int enum_value = 0;
       enum_value <=
       static_cast<int>(TrustSafetySentimentService::FeatureArea::kMaxValue);
       ++enum_value) {
    // Skip deprecated PrivacySandbox3 values.
    if (enum_value >= 4 && enum_value <= 9) {
      continue;
    }
    auto feature_area =
        static_cast<TrustSafetySentimentService::FeatureArea>(enum_value);
    if (TrustSafetySentimentService::VersionCheck(feature_area)) {
      EXPECT_TRUE(TrustSafetySentimentService::ProbabilityCheck(feature_area))
          << "Feature area: " << static_cast<int>(feature_area);
    }
  }
}

TEST_F(TrustSafetySentimentServiceTest, Eligibility_V1FeatureWhileV2Enabled) {
  // A survey from V1 only is not shown because V2 is enabled.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  feature_list()->InitWithFeaturesAndParameters(
      {{features::kTrustSafetySentimentSurvey,
        {
            {"min-time-to-prompt", params.min_time_to_prompt},
            {"max-time-to-prompt", params.max_time_to_prompt},
            {"ntp-visits-min-range", params.ntp_visits_min_range},
            {"ntp-visits-max-range", params.ntp_visits_max_range},
            {"trusted-surface-probability", params.trusted_surface_probability},
            {"trusted-surface-trigger-id", params.trusted_surface_trigger_id},
        }},
       {features::kTrustSafetySentimentSurveyV2, {}}},
      {});

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});

  service()->OpenedNewTabPage();

  // Survey should not shown be shown as triggered because v2 enabled.
  CheckHistograms({}, {});
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Disable V2 and now the same trigger should work.
  feature_list()->Reset();
  SetupFeatureParameters(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings},
                  {TrustSafetySentimentService::FeatureArea::kPrivacySettings});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPrivacySettings, 2}});
}

TEST_F(TrustSafetySentimentServiceTest, V2_TrustedSurface) {
  // A survey from version 2 is only shown if the right conditions are met.
  FeatureParamsV2 params;
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "2m";
  params.max_time_to_prompt = "4m";
  params.ntp_visits_min_range = "2";
  params.ntp_visits_max_range = "2";
  SetupFeatureParametersV2(params);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  service()->OpenedNewTabPage();
  service()->OpenedNewTabPage();

  // Survey should not shown because although the ntp visits condition is met,
  // the time is not.
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {});
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  task_environment()->AdvanceClock(base::Minutes(3));
  // Assert the V2 survey is called and not the V1.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface, _, _, _, _))
      .Times(0);
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2TrustedSurface, _, _, _, _));

  // A survey should be shown because we are now within the right time.
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {TrustSafetySentimentService::FeatureArea::kTrustedSurface});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kTrustedSurface, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, V2_SafetyCheck) {
  // Running the safety check is considered a trigger, and should make a user
  // eligible to receive a survey.
  FeatureParamsV2 params;
  params.safety_check_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // Running safety check was previously part of PrivacySettings, so assure only
  // the correct histograms and survey are triggered for V2.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _))
      .Times(0);
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2SafetyCheck, _, _, _, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kSafetyCheck},
                  {TrustSafetySentimentService::FeatureArea::kSafetyCheck});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kSafetyCheck, 1},
       {TrustSafetySentimentService::FeatureArea::kPrivacySettings, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, V2_PasswordCheck) {
  // Running password check should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.password_check_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PasswordCheck, _, _, _, _));
  service()->RanPasswordCheck();
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPasswordCheck},
                  {TrustSafetySentimentService::FeatureArea::kPasswordCheck});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPasswordCheck, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, V2_BrowsingData) {
  // Clearing history through CBD should make user eligible to receive a survey.
  FeatureParamsV2 params;
  params.browsing_data_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  std::vector<std::pair<browsing_data::BrowsingDataType, SurveyBitsData>>
      datatypes = {{browsing_data::BrowsingDataType::HISTORY,
                    {{"Deleted history", true},
                     {"Deleted downloads", false},
                     {"Deleted autofill form data", false}}},
                   {browsing_data::BrowsingDataType::DOWNLOADS,
                    {{"Deleted history", false},
                     {"Deleted downloads", true},
                     {"Deleted autofill form data", false}}},
                   {browsing_data::BrowsingDataType::FORM_DATA,
                    {{"Deleted history", false},
                     {"Deleted downloads", false},
                     {"Deleted autofill form data", true}}}};

  for (const auto& datatype : datatypes) {
    // The correct survey should be launched.
    EXPECT_CALL(*mock_hats_service(),
                LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2BrowsingData, _, _,
                             datatype.second, _));
    service()->ClearedBrowsingData(datatype.first);
    service()->OpenedNewTabPage();
    testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  }
}

TEST_F(TrustSafetySentimentServiceTest, V2_BrowsingData_NotInterested) {
  // Clearing a BrowsingDataType that we are not interested in should not
  // trigger a survey.
  FeatureParamsV2 params;
  params.browsing_data_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // No browsing data survey should be launched.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2BrowsingData, _, _, _, _))
      .Times(0);
  service()->ClearedBrowsingData(browsing_data::BrowsingDataType::SITE_DATA);
  service()->OpenedNewTabPage();
  CheckHistograms({}, {});
  CheckCallTriggerOccurredHistogram({});
}

TEST_F(TrustSafetySentimentServiceTest, V2_PrivacyGuide) {
  // Finishing the privacy guide is considered a trigger, and should make a user
  // eligible to receive a survey.
  FeatureParamsV2 params;
  params.privacy_guide_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PrivacyGuide, _, _, _, _));
  service()->FinishedPrivacyGuide();
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacyGuide},
                  {TrustSafetySentimentService::FeatureArea::kPrivacyGuide});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPrivacyGuide, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, V2_ControlGroup) {
  // If a user is in the control group, they should be eligible to see a survey
  // if they reached a certain threshold of session length, and only once.
  FeatureParamsV2 params;
  params.control_group_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  params.min_session_time = "30s";
  SetupFeatureParametersV2(params);

  // A survey should not be launched because the session threshold was not
  // surpassed.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  base::TimeTicks session_start = base::TimeTicks::Now();
  service()->OnSessionStarted(session_start);
  task_environment()->AdvanceClock(base::Seconds(10));
  base::TimeTicks session_end = base::TimeTicks::Now();
  service()->OnSessionEnded(session_end - session_start, session_end);
  service()->OpenedNewTabPage();
  CheckHistograms({}, {});
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // A survey should be launched because the session threshold was surpassed.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2ControlGroup, _, _, _, _));
  session_start = base::TimeTicks::Now();
  service()->OnSessionStarted(session_start);
  task_environment()->AdvanceClock(base::Seconds(40));
  session_end = base::TimeTicks::Now();
  service()->OnSessionEnded(session_end - session_start, session_end);
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kControlGroup},
                  {TrustSafetySentimentService::FeatureArea::kControlGroup});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kControlGroup, 1}});
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // A second valid trigger should not launch a survey because we have already
  // performed one dice roll for this user.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  session_start = base::TimeTicks::Now();
  service()->OnSessionStarted(session_start);
  task_environment()->AdvanceClock(base::Seconds(40));
  session_end = base::TimeTicks::Now();
  service()->OnSessionEnded(session_end - session_start, session_end);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, V2_PrivacySandbox4ConsentAccept) {
  // Accepting Privacy Sandbox 4 consent is considered a trigger, and should
  // make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.privacy_sandbox_4_consent_accept_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentAccept,
                   _, _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4ConsentAccept);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, V2_PrivacySandbox4ConsentDecline) {
  // Declining Privacy Sandbox 4 consent is considered a trigger, and should
  // make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.privacy_sandbox_4_consent_decline_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4ConsentDecline,
                   _, _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4ConsentDecline);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, V2_PrivacySandbox4NoticeOk) {
  // Acknowledging the Privacy Sandbox 4 notice is considered a trigger, and
  // should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.privacy_sandbox_4_notice_ok_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeOk, _, _,
                   _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeOk);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, V2_PrivacySandbox4NoticeSettings) {
  // Going to settings from the Privacy Sandbox 4 notice is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.privacy_sandbox_4_notice_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PrivacySandbox4NoticeSettings,
                   _, _, _, _));
  service()->InteractedWithPrivacySandbox4(
      TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeSettings);
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, V2_SafeBrowsingInterstitial) {
  // Making a final decision on a safe browsing interstitial is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.safe_browsing_interstitial_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2SafeBrowsingInterstitial, _,
                   _, _, _));
  service()->InteractedWithSafeBrowsingInterstitial(
      true, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  service()->OpenedNewTabPage();
  CheckHistograms(
      {TrustSafetySentimentService::FeatureArea::kSafeBrowsingInterstitial},
      {TrustSafetySentimentService::FeatureArea::kSafeBrowsingInterstitial});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kSafeBrowsingInterstitial,
        1}});
}

TEST_F(TrustSafetySentimentServiceTest, V2_DownloadWarningUI) {
  // Making a final decision on a download warning is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.download_warning_ui_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2DownloadWarningUI, _,
                           _, _, _));
  service()->InteractedWithDownloadWarningUI(
      DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
      DownloadItemWarningData::WarningAction::PROCEED);
  service()->OpenedNewTabPage();
  CheckHistograms(
      {TrustSafetySentimentService::FeatureArea::kDownloadWarningUI},
      {TrustSafetySentimentService::FeatureArea::kDownloadWarningUI});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kDownloadWarningUI, 1}});
}

TEST_F(TrustSafetySentimentServiceTest, PasswordProtectionUINonPasswordChange) {
  // Making a final decision on a password protection UI is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.password_protection_ui_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI,
                           _, _, _, _));
  service()->PhishedPasswordUpdateNotClicked(
      PasswordProtectionUIType::PAGE_INFO,
      PasswordProtectionUIAction::IGNORE_WARNING);
  service()->OpenedNewTabPage();
  CheckHistograms(
      {TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI},
      {TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI, 1}});
}

TEST_F(TrustSafetySentimentServiceTest,
       PasswordProtectionUIPasswordChangeClickedNotCompleted) {
  // Making a final decision on a password protection UI is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.password_protection_ui_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI,
                           _, _, _, _));
  service()->ProtectResetOrCheckPasswordClicked(
      PasswordProtectionUIType::PAGE_INFO);
  task_environment()->AdvanceClock(kPasswordChangeInactivity);
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  CheckHistograms(
      {TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI},
      {TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI, 1}});
}

TEST_F(TrustSafetySentimentServiceTest,
       PasswordProtectionUIPasswordChangeClickedAndCompleted) {
  // Making a final decision on a password protection UI is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.password_protection_ui_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI,
                           _, _, _, _));
  service()->ProtectResetOrCheckPasswordClicked(
      PasswordProtectionUIType::PAGE_INFO);
  service()->PhishedPasswordUpdateFinished();
  service()->OpenedNewTabPage();
  CheckHistograms(
      {TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI},
      {TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI});
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI, 1}});
}

TEST_F(TrustSafetySentimentServiceTest,
       PasswordProtectionUIPasswordChangeThenNonPasswordChange) {
  // Making a final decision on a password protection UI is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.password_protection_ui_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI,
                           _, _, _, _));
  service()->ProtectResetOrCheckPasswordClicked(
      PasswordProtectionUIType::PAGE_INFO);
  service()->PhishedPasswordUpdateNotClicked(
      PasswordProtectionUIType::PAGE_INFO, PasswordProtectionUIAction::CLOSE);
  service()->PhishedPasswordUpdateFinished();
  service()->OpenedNewTabPage();
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI, 2}});
}

TEST_F(TrustSafetySentimentServiceTest,
       PasswordProtectionUIPasswordChangeThenNonPasswordChange2) {
  // Making a final decision on a password protection UI is considered a
  // trigger, and should make a user eligible to receive a survey.
  FeatureParamsV2 params;
  params.password_protection_ui_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParametersV2(params);

  // The correct survey should be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyV2PasswordProtectionUI,
                           _, _, _, _));
  service()->ProtectResetOrCheckPasswordClicked(
      PasswordProtectionUIType::PAGE_INFO);
  service()->PhishedPasswordUpdateNotClicked(
      PasswordProtectionUIType::PAGE_INFO, PasswordProtectionUIAction::CLOSE);
  service()->PhishedPasswordUpdateFinished();
  task_environment()->AdvanceClock(kPasswordChangeInactivity);
  service()->OpenedNewTabPage();
  CheckCallTriggerOccurredHistogram(
      {{TrustSafetySentimentService::FeatureArea::kPasswordProtectionUI, 2}});
}
