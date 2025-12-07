// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
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
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/test_web_ui.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

class Profile;

using safe_browsing::SafeBrowsingState;

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
          HatsService::NavigationBehavior::REQUIRE_SAME_ORIGIN, _, _, _, _))
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
          HatsService::NavigationBehavior::REQUIRE_SAME_ORIGIN, _, _, _, _))
      .Times(1);
  base::Value::List args;
  args.Append(static_cast<int>(
      HatsHandler::TrustSafetyInteraction::COMPLETED_PRIVACY_GUIDE));
  handler()->HandleTrustSafetyInteractionOccurred(args);
  task_environment()->RunUntilIdle();
}

TEST_F(HatsHandlerTest,
       HandleSecurityPageHatsRequest_NoSurveyIfSurveysDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, false);

  // Check that the survey is not launched if surveys are disabled by pref.
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerSettingsSecurity, _, _, _, _, _, _))
      .Times(0);

  base::Value::List args;
  args.Append(base::Value::List());  // No interactions
  args.Append(static_cast<int>(SafeBrowsingState::STANDARD_PROTECTION));
  // Set the time spent on the page to 20,000 milliseconds, which is longer than
  // the configured value from Finch, 15,000 milliseconds.
  args.Append(20000);
  args.Append(
      static_cast<int>(HatsHandler::SecuritySettingsBundleSetting::STANDARD));

  handler()->HandleSecurityPageHatsRequest(args);
  task_environment()->RunUntilIdle();
}

TEST_F(HatsHandlerTest,
       HandleSecurityPageHatsRequest_NoSurveyIfInsufficientTimeOnPage) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);

  // Check that the survey is not launched if the user has not spent enough
  // time on the page.
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerSettingsSecurity, _, _, _, _, _, _))
      .Times(0);

  base::Value::List args;
  args.Append(base::Value::List());  // No interactions
  args.Append(static_cast<int>(SafeBrowsingState::STANDARD_PROTECTION));
  // Set the time spent on the page to 10,000 milliseconds, which is shorter
  // than the configured value from Finch, 15,000 milliseconds.
  args.Append(10000);
  args.Append(
      static_cast<int>(HatsHandler::SecuritySettingsBundleSetting::STANDARD));

  handler()->HandleSecurityPageHatsRequest(args);
  task_environment()->RunUntilIdle();
}

TEST_F(HatsHandlerTest,
       HandleSecurityPageHatsRequest_PassesArgumentsToHatsService) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, true);

  SurveyStringData expected_product_specific_data = {
      {"Security page user actions",
       "enhanced_bundle_radio_button_clicked, "
       "safe_browsing_row_expanded, "
       "enhanced_safe_browsing_radio_button_clicked"},
      {"Safe browsing setting when security page opened",
       "standard_protection"},
      {"Security settings bundle setting when security page opened",
       "standard_protection"},
      {"Safe browsing setting when security page closed",
       "enhanced_protection"},
      {"Security settings bundle setting when security page closed",
       "enhanced_protection"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
      {"Client channel", "stable"},
#else
      {"Client channel", "unknown"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"Time on page (bucketed seconds)",
       base::NumberToString(ukm::GetExponentialBucketMinForUserTiming(20))},
  };

  // Check that triggering the security page handler function will trigger HaTS
  // correctly.
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerSettingsSecurity, _, _, _,
                           expected_product_specific_data, _, _))
      .Times(1);

  base::Value::List interactions;
  interactions.Append(static_cast<int>(HatsHandler::SecurityPageV2Interaction::
                                           ENHANCED_BUNDLE_RADIO_BUTTON_CLICK));
  interactions.Append(static_cast<int>(
      HatsHandler::SecurityPageV2Interaction::SAFE_BROWSING_ROW_EXPANDED));
  interactions.Append(
      static_cast<int>(HatsHandler::SecurityPageV2Interaction::
                           ENHANCED_SAFE_BROWSING_RADIO_BUTTON_CLICK));

  base::Value::List args;
  args.Append(std::move(interactions));
  args.Append(static_cast<int>(SafeBrowsingState::STANDARD_PROTECTION));
  // Set the time spent on the page to 20,000 milliseconds, which is longer than
  // the configured value from Finch, 15,000 milliseconds.
  args.Append(20000);
  args.Append(
      static_cast<int>(HatsHandler::SecuritySettingsBundleSetting::STANDARD));

  // The "current" settings prefs are read by the handler to determine the state
  // of the page when the survey is requested.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  profile()->GetPrefs()->SetInteger(
      prefs::kSecuritySettingsBundle,
      static_cast<int>(HatsHandler::SecuritySettingsBundleSetting::ENHANCED));

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

}  // namespace settings
