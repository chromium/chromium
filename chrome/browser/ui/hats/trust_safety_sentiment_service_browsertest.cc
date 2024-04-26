// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time_override.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using ::testing::_;

namespace {

std::unique_ptr<KeyedService> BuildSentimentServiceForTesting(
    content::BrowserContext* context) {
  return std::make_unique<TrustSafetySentimentService>(
      static_cast<Profile*>(context));
}

}  // namespace

class TrustSafetySentimentServiceBrowserTest : public InProcessBrowserTest {
 public:
  TrustSafetySentimentServiceBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kTrustSafetySentimentSurvey,
        {{"trusted-surface-probability", "1.0"}});
  }

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
    TrustSafetySentimentServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&BuildSentimentServiceForTesting));
    EXPECT_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

  void TearDownOnMainThread() override { mock_hats_service_ = nullptr; }

  void OpenPageInfo() {
    ShowPageInfoDialog(browser()->tab_strip_model()->GetActiveWebContents(),
                       base::DoNothing());
  }

  void ClosePageInfo() {
    PageInfoBubbleView::GetPageInfoBubbleForTesting()
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    base::RunLoop().RunUntilIdle();
  }

  void ChangePermission() {
    PageInfo::PermissionInfo permission;
    permission.type = ContentSettingsType::NOTIFICATIONS;
    permission.setting = ContentSetting::CONTENT_SETTING_BLOCK;
    permission.default_setting = ContentSetting::CONTENT_SETTING_ASK;

    auto* bubble = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    bubble->presenter_for_testing()->OnSitePermissionChanged(
        permission.type, permission.setting, permission.requesting_origin,
        permission.is_one_time);
  }

  void OpenEnoughNewTabs() {
    for (int i = 0;
         i < features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get();
         i++) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), GURL(chrome::kChromeUINewTabURL),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(TrustSafetySentimentServiceBrowserTest,
                       PageInfoTriggersSurvey_NoInteraction) {
  // Check that after opening Page Info for the required time, then performing
  // the required eligibility actions, a survey is requested from the HaTS
  // service.
  SurveyBitsData expected_product_specific_data = {
      {"Interacted with Page Info", false}};
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface, _, _,
                           expected_product_specific_data, _));
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight();
        },
        nullptr, nullptr);
    OpenPageInfo();
  }

  // Wait for the required time before closing.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 features::kTrustSafetySentimentSurveyTrustedSurfaceTime.Get();
        },
        nullptr, nullptr);
    ClosePageInfo();
  }

  // Open the maximum number of required tabs, after waiting for the minimum
  // prompt time.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 features::kTrustSafetySentimentSurveyTrustedSurfaceTime.Get() +
                 features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
        },
        nullptr, nullptr);
    OpenEnoughNewTabs();
  }
}

IN_PROC_BROWSER_TEST_F(TrustSafetySentimentServiceBrowserTest,
                       PageInfoTriggersSurvey_Interaction) {
  // Check that interacting with page info (through a reported change to
  // permissions in this instance) removes the time requirement, and also
  // changes the product specific data accordingly.
  SurveyBitsData expected_product_specific_data = {
      {"Interacted with Page Info", true}};
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface, _, _,
                           expected_product_specific_data, _));

  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight();
        },
        nullptr, nullptr);
    OpenPageInfo();
    ChangePermission();
    ClosePageInfo();
  }

  {
    base::subtle::ScopedTimeClockOverrides override(
        []() {
          return base::subtle::TimeNowIgnoringOverride().LocalMidnight() +
                 features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
        },
        nullptr, nullptr);
    OpenEnoughNewTabs();
  }
}
