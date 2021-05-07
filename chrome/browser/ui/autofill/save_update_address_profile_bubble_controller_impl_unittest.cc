// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
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
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillAddressProfileSavePrompt);

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

// This is testing that when the SaveAddressProfilePromptOptions has the
// show_prompt set to true, the bubble should be visible.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       BubbleShouldBeVisibleWithShowPrompt) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      callback.Get());
  // Bubble is visible and active
  EXPECT_TRUE(controller()->GetSaveBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

// This is testing that when the SaveAddressProfilePromptOptions has the
// show_prompt set to false, the bubble should be invisible.
TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
       BubbleShouldBeInvisibleWithoutShowPrompt) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(
      profile, /*original_profile=*/nullptr,
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = false},
      callback.Get());
  // Bubble is invisible but active
  EXPECT_FALSE(controller()->GetSaveBubbleView());
  EXPECT_TRUE(controller()->IsBubbleActive());
}

}  // namespace autofill
