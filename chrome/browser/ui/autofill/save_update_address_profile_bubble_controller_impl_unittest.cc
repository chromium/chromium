// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveUpdateAddressProfileBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  SaveUpdateAddressProfileBubbleControllerImplTest() = default;
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SaveUpdateAddressProfileBubbleControllerImpl::CreateForWebContents(
        web_contents);
  }

  SaveUpdateAddressProfileBubbleControllerImpl* controller() {
    return SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
};

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile));
  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          testing::_));
  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

// This is testing that closing all tabs (which effectively destroys the web
// contents) will trigger the save callback with kIgnored decions if the users
// hasn't interacted with the prompt already.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       WebContentsDestroyedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // There is only now tab open, so the active web contents, are the
  // controller's web contents.
  content::WebContents* controller_web_contents =
      tab_strip_model->GetActiveWebContents();

  // Now add another tab, and close the controller tab to make sure the window
  // remains open. This should destroy the web contents of the controller and
  // invoke the callback with a decision kIgnored.
  AddTab(browser(), GURL("http://foo.com/"));
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_CALL(callback,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  testing::_));
  // Close controller tab.
  EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(
      tab_strip_model->GetIndexOfWebContents(controller_web_contents),
      TabCloseTypes::CLOSE_USER_GESTURE));
  EXPECT_EQ(1, tab_strip_model->count());
}

// This is testing that when the SaveAddressProfilePromptOptions has the
// show_prompt set to true, the bubble should be visible.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       BubbleShouldBeVisibleWithShowPrompt) {
  AutofillProfile profile = test::GetFullProfile();
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      /*address_profile_save_prompt_callback=*/base::DoNothing());

  // Bubble is visible and active
  EXPECT_TRUE(controller()->GetBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when the SaveAddressProfilePromptOptions has the
// show_prompt set to false, the bubble should be invisible.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       BubbleShouldBeInvisibleWithoutShowPrompt) {
  AutofillProfile profile = test::GetFullProfile();
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = false},
      /*address_profile_save_prompt_callback=*/base::DoNothing());
  // Bubble is invisible but active
  EXPECT_FALSE(controller()->GetBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when a second prompt comes while another prompt is
// shown, the controller will ignore it, and inform the backend that the second
// prompt has been auto declined.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       SecondPromptWillBeAutoDeclinedWhileFirstIsVisible) {
  AutofillProfile profile = test::GetFullProfile();

  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      /*address_profile_save_prompt_callback=*/base::DoNothing());

  // Second prompt should be auto declined.
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
          testing::_));
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());
}

// This is testing that when a second prompt comes while another prompt is in
// progress but not shown, the controller will inform the backend that the first
// process is ignored.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       FirstHiddenPromptWillBeIgnoredWhenSecondPromptArrives) {
  AutofillProfile profile = test::GetFullProfile();

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());
  controller()->OnBubbleClosed();

  // When second prompt comes, the first one will be ignored.
  EXPECT_CALL(callback,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  testing::_));
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      /*address_profile_save_prompt_callback=*/base::DoNothing());
}

}  // namespace autofill
