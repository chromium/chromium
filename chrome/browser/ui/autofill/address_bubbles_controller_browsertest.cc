// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"

namespace autofill {

using ::testing::_;
using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

class AddressBubblesControllerBrowserTest
    : public InProcessBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  AddressBubblesControllerBrowserTest(): base::test::WithFeatureOverride(
            features::kAutofillShowBubblesBasedOnPriorities) {
    scoped_features_.InitAndEnableFeature(
        autofill::features::kAutofillAddressUserDeclinedSaveSurvey);
  }

  AddressBubblesControllerBrowserTest(
      const AddressBubblesControllerBrowserTest&) = delete;
  AddressBubblesControllerBrowserTest& operator=(
      const AddressBubblesControllerBrowserTest&) = delete;
  ~AddressBubblesControllerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
    side_panel_ui->SetNoDelaysForTesting(true);
    side_panel_ui->DisableAnimationsForTesting();
  }

  bool IsBubbleManagerEnabled() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_features_;

  raw_ptr<content::WebContents> tab_web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  AddressBubblesController* tab_controller() {
    return AddressBubblesController::FromWebContents(tab_web_contents());
  }
};

IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt);
}

// This is testing that the callback is invoked when the dialog is triggered in
// the side panel. It covers the regression found in crbug.com/401068467.
IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       DialogAcceptedInvokesCallbackForSidePanel) {
  if (IsBubbleManagerEnabled()) {
    GTEST_SKIP() << "Bubble Manager is incompatible with side panel";
  }
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  content::WebContents* side_panel_web_contents =
      side_panel_ui->GetWebContentsForTest(SidePanelEntry::Id::kReadingList);
  side_panel_ui->Show(SidePanelEntry::Id::kReadingList);
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      side_panel_web_contents, profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  AddressBubblesController::FromWebContents(side_panel_web_contents)
      ->OnUserDecision(AutofillClient::AddressPromptUserDecision::kAccepted,
                       std::nullopt);
}

IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined,
                  Property(&profile_ref::has_value, false)));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       DeclinedSaveTriggersSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  auto empty_profile = AutofillProfile(AddressCountryCode("US"));
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), empty_profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAddressUserDeclinedSave,
          _, _, _, _, _, _, _, _, _));
  EXPECT_CALL(
      callback,
      Run(AutofillClient::AddressPromptUserDecision::kDeclined, _));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       DeclinedSaveWithProfileDoesNotTriggerSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), test::GetFullProfile(), /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/true, callback.Get());

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAddressUserDeclinedSave,
          _, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined, _));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kDeclined, std::nullopt);
}

IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       AcceptedSaveDoesNotTriggerSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  auto empty_profile = AutofillProfile(AddressCountryCode("US"));
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), empty_profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAddressUserDeclinedSave,
          _, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      callback,
      Run(AutofillClient::AddressPromptUserDecision::kAccepted, _));
  tab_controller()->OnUserDecision(
      AutofillClient::AddressPromptUserDecision::kAccepted, std::nullopt);
}
#endif

// This is testing that closing all tabs (which effectively destroys the web
// contents) will trigger the save callback with kIgnored decions if the users
// hasn't interacted with the prompt already.
IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       WebContentsDestroyedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  CHECK_EQ(1, tab_strip_model->count());
  // There is only now tab open, so the active web contents, are the
  // controller's web contents.
  content::WebContents* controller_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Now add another tab, and close the controller tab to make sure the window
  // remains open. This should destroy the web contents of the controller and
  // invoke the callback with a decision kIgnored.
  GURL url(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  // Close controller tab.
  int previous_tab_count = browser()->tab_strip_model()->count();
  browser()->tab_strip_model()->CloseWebContentsAt(
      tab_strip_model->GetIndexOfWebContents(controller_web_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());
}

// This is testing that the bubble is visible and active when shown.
IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       BubbleShouldBeVisibleByDefault) {
  AutofillProfile profile = test::GetFullProfile();
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{},
      /*callback=*/base::DoNothing());

  // Bubble is visible and active
  EXPECT_TRUE(tab_controller()->GetBubbleView());
  EXPECT_TRUE(tab_controller()->IsBubbleActive());
}

// This is testing that when a second prompt comes while another prompt is
// shown, the controller will ignore it, and inform the backend that the second
// prompt has been auto declined.
IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       SecondPromptWillBeAutoDeclinedWhileFirstIsVisible) {
  AutofillProfile profile = test::GetFullProfile();

  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{},
      /*callback=*/base::DoNothing());

  // Second prompt should be auto declined.
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());
}

// This is testing that when a second prompt comes while another prompt is in
// progress but not shown, the controller will inform the backend that the first
// process is ignored.
IN_PROC_BROWSER_TEST_P(AddressBubblesControllerBrowserTest,
                       FirstHiddenPromptWillBeIgnoredWhenSecondPromptArrives) {
  AutofillProfile profile = test::GetFullProfile();

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{}, callback.Get());
  tab_controller()->OnBubbleClosed();

  // When second prompt comes, the first one will be ignored.
  EXPECT_CALL(callback, Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                            Property(&profile_ref::has_value, false)));
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      tab_web_contents(), profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressBubbleType::kSave,
      /*user_has_any_profile_saved=*/{},
      /*callback=*/base::DoNothing());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AddressBubblesControllerBrowserTest);

}  // namespace autofill
