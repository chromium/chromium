// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

class Profile;

namespace settings {

class HatsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  HatsHandlerTest() {
    base::test::FeatureRefAndParams settings_privacy{
        features::kHappinessTrackingSurveysForDesktopSettingsPrivacy,
        {{"settings-time", "15s"}}};
    base::test::FeatureRefAndParams privacy_guide{
        features::kHappinessTrackingSurveysForDesktopPrivacyGuide,
        {{"settings-time", "15s"}}};
    base::test::FeatureRefAndParams security_page{
        features::kHappinessTrackingSurveysForSecurityPage,
        {{"security-page-time", "15s"}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {settings_privacy, privacy_guide, security_page}, {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    handler_ = std::make_unique<HatsHandler>();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));

    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  HatsHandler* handler() { return handler_.get(); }
  raw_ptr<MockHatsService, DanglingUntriaged> mock_hats_service_;
  raw_ptr<MockTrustSafetySentimentService, DanglingUntriaged>
      mock_sentiment_service_;

 protected:
  // This should only be accessed in the test constructor, to avoid race
  // conditions with other threads.
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<HatsHandler> handler_;
};

TEST_F(HatsHandlerTest, PrivacySettingsHats) {
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  SurveyBitsData expected_product_specific_data = {
      {"3P cookies blocked", true}};

  // Check that both interacting with the privacy card, and running Safety Check
  // result in a survey request with the appropriate product specific data.
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerSettingsPrivacy, web_contents(), 15000,
          expected_product_specific_data, _,
          HatsService::NavigationBehaviour::REQUIRE_SAME_ORIGIN, _, _, _, _))
      .Times(2);
  base::Value::List args;
  args.Append(
      static_cast<int>(HatsHandler::TrustSafetyInteraction::USED_PRIVACY_CARD));
  handler()->HandleTrustSafetyInteractionOccurred(args);
  task_environment()->RunUntilIdle();

  args[0] = base::Value(
      static_cast<int>(HatsHandler::TrustSafetyInteraction::RAN_SAFETY_CHECK));
  handler()->HandleTrustSafetyInteractionOccurred(args);
  task_environment()->RunUntilIdle();
}

TEST_F(HatsHandlerTest, PrivacyGuideHats) {
  // Check that completing a privacy guide triggers a privacy guide hats.
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPrivacyGuide, web_contents(), 15000, _, _,
          HatsService::NavigationBehaviour::REQUIRE_SAME_ORIGIN, _, _, _, _))
      .Times(1);
  base::Value::List args;
  args.Append(static_cast<int>(
      HatsHandler::TrustSafetyInteraction::COMPLETED_PRIVACY_GUIDE));
  handler()->HandleTrustSafetyInteractionOccurred(args);
  task_environment()->RunUntilIdle();
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_HandleSecurityPageHatsRequestPassesArgumentsToHatsService \
  DISABLED_HandleSecurityPageHatsRequestPassesArgumentsToHatsService
#else
#define MAYBE_HandleSecurityPageHatsRequestPassesArgumentsToHatsService \
  HandleSecurityPageHatsRequestPassesArgumentsToHatsService
#endif
TEST_F(HatsHandlerTest,
       MAYBE_HandleSecurityPageHatsRequestPassesArgumentsToHatsService) {
  SurveyStringData expected_product_specific_data = {
      {"Security Page User Action", "enhanced_protection_radio_button_clicked"},
      {"Safe Browsing Setting Before Trigger", "standard_protection"},
      {"Safe Browsing Setting After Trigger", "standard_protection"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"Client Channel", "stable"},
#else
      {"Client Channel", "unknown"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"Time On Page", "20000"},
  };

  // Check that triggering the security page handler function will trigger HaTS
  // correctly.
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerSettingsSecurity, _, _, _,
                           expected_product_specific_data))
      .Times(1);

  base::Value::List args;
  args.Append(static_cast<int>(
      HatsHandler::SecurityPageInteraction::RADIO_BUTTON_ENHANCED_CLICK));
  args.Append(static_cast<int>(HatsHandler::SafeBrowsingSetting::STANDARD));
  // Set the time spent on the page to 20,000 milliseconds, which is longer than
  // the configured value from Finch, 15,000 milliseconds.
  args.Append(20000);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);

  handler()->HandleSecurityPageHatsRequest(args);
  task_environment()->RunUntilIdle();
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNoInteraction \
  DISABLED_HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNoInteraction
#else
#define MAYBE_HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNoInteraction \
  HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNoInteraction
#endif
TEST_F(
    HatsHandlerTest,
    MAYBE_HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNoInteraction) {
  // Reconfigure the feature parameter to require interaction to launch the
  // survey.
  base::test::FeatureRefAndParams security_page{
      features::kHappinessTrackingSurveysForSecurityPage,
      {{"security-page-time", "15s"},
       {"security-page-require-interaction", "true"}}};
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters({security_page}, {});

  // Verify that if there are no interactions on the security page but user
  // interactions are required through finch, the survey will not be shown.
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerSettingsSecurity, _, _, _, _))
      .Times(0);

  base::Value::List args;
  args.Append(
      static_cast<int>(HatsHandler::SecurityPageInteraction::NO_INTERACTION));
  args.Append(static_cast<int>(HatsHandler::SafeBrowsingSetting::STANDARD));
  args.Append(20000);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);

  handler()->HandleSecurityPageHatsRequest(args);
  task_environment()->RunUntilIdle();
}

TEST_F(HatsHandlerTest, TrustSafetySentimentInteractions) {
  // Check that interactions relevant to the T&S sentiment service are
  // correctly reported.
  EXPECT_CALL(*mock_sentiment_service_,
              InteractedWithPrivacySettings(web_contents()))
      .Times(1);
  base::Value::List args;
  args.Append(
      static_cast<int>(HatsHandler::TrustSafetyInteraction::USED_PRIVACY_CARD));
  handler()->HandleTrustSafetyInteractionOccurred(args);

  EXPECT_CALL(*mock_sentiment_service_, RanSafetyCheck()).Times(1);
  args[0] = base::Value(
      static_cast<int>(HatsHandler::TrustSafetyInteraction::RAN_SAFETY_CHECK));
  handler()->HandleTrustSafetyInteractionOccurred(args);
}

class HatsHandlerParamTest : public HatsHandlerTest,
                             public testing::WithParamInterface<bool> {};

TEST_P(HatsHandlerParamTest, AdPrivacyHats) {
  auto cookie_setting =
      GetParam() ? content_settings::CookieControlsMode::kBlockThirdParty
                 : content_settings::CookieControlsMode::kIncognitoOnly;
  profile()->GetPrefs()->SetInteger(prefs::kCookieControlsMode,
                                    static_cast<int>(cookie_setting));
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled,
                                    GetParam());
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled,
                                    GetParam());
  profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled, GetParam());
  SurveyBitsData expected_product_specific_data = {
      {"3P cookies blocked", GetParam()},
      {"Topics enabled", GetParam()},
      {"Fledge enabled", GetParam()},
      {"Ad Measurement enabled", GetParam()}};

  auto interaction_to_survey =
      std::map<HatsHandler::TrustSafetyInteraction, std::string>{
          {HatsHandler::TrustSafetyInteraction::OPENED_AD_PRIVACY,
           kHatsSurveyTriggerM1AdPrivacyPage},
          {HatsHandler::TrustSafetyInteraction::OPENED_TOPICS_SUBPAGE,
           kHatsSurveyTriggerM1TopicsSubpage},
          {HatsHandler::TrustSafetyInteraction::OPENED_FLEDGE_SUBPAGE,
           kHatsSurveyTriggerM1FledgeSubpage},
          {HatsHandler::TrustSafetyInteraction::OPENED_AD_MEASUREMENT_SUBPAGE,
           kHatsSurveyTriggerM1AdMeasurementSubpage},
      };

  for (const auto& [interaction, survey] : interaction_to_survey) {
    EXPECT_CALL(
        *mock_hats_service_,
        LaunchDelayedSurveyForWebContents(
            survey, web_contents(), 20000, expected_product_specific_data, _,
            HatsService::NavigationBehaviour::REQUIRE_SAME_ORIGIN, _, _, _, _));
    base::Value::List args;
    args.Append(static_cast<int>(interaction));
    handler()->HandleTrustSafetyInteractionOccurred(args);
    task_environment()->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
  }
}

INSTANTIATE_TEST_SUITE_P(AdPrivacy, HatsHandlerParamTest, testing::Bool());

}  // namespace settings
