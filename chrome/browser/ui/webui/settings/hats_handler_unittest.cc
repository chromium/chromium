// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace settings {

class HatsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
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
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  HatsHandler* handler() { return handler_.get(); }
  MockHatsService* mock_hats_service_;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<HatsHandler> handler_;
};

TEST_F(HatsHandlerTest, HandleTryShowHatsSurvey) {
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  std::map<std::string, bool> expected_product_specific_data = {
      {"3P cookies blocked", true}, {"Privacy Sandbox enabled", false}};

  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerSettingsPrivacy,
                                       web_contents(), 20000, expected_product_specific_data));
  base::ListValue args;
  handler()->HandleTryShowHatsSurvey(&args);
  task_environment()->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);

  // Enable targeting for users who have not seen the Privacy Sandbox page and
  // ensure the handler does not attempt to launch the survey.
  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(testing::_, testing::_,
                                                testing::_, testing::_))
      .Times(0);

  base::test::ScopedFeatureList::FeatureAndParams feature_and_params{
      features::kHappinessTrackingSurveysForDesktopSettingsPrivacy,
      {{"no-sandbox", "true"}}};
  scoped_feature_list_.InitWithFeaturesAndParameters({feature_and_params}, {});

  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxPageViewed, true);

  handler()->HandleTryShowHatsSurvey(&args);
  task_environment()->RunUntilIdle();
}

TEST_F(HatsHandlerTest, HandleTryShowPrivacySandboxHatsSurvey) {
  // Check that the handler correctly forwards the survey request to the
  // HaTS service and also includes the appropriate product specific data.
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  std::map<std::string, bool> expected_product_specific_data = {
      {"3P cookies blocked", true}, {"Privacy Sandbox enabled", false}};
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerPrivacySandbox,
                                       web_contents(), 20000, expected_product_specific_data));
  base::ListValue args;
  handler()->HandleTryShowPrivacySandboxHatsSurvey(&args);
  task_environment()->RunUntilIdle();
}

}  // namespace settings
