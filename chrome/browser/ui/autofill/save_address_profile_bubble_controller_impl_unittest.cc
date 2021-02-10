// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller_impl.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveAddressProfileBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  SaveAddressProfileBubbleControllerImplTest() = default;
  void SetUp() override {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillAddressProfileSavePrompt);

    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SaveAddressProfileBubbleControllerImpl::CreateForWebContents(web_contents);
  }

  SaveAddressProfileBubbleControllerImpl* controller() {
    return SaveAddressProfileBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
};

TEST_F(SaveAddressProfileBubbleControllerImplTest,
       DialogAcceptedInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(profile, callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile));
  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
}

TEST_F(SaveAddressProfileBubbleControllerImplTest,
       DialogCancelledInvokesCallback) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  controller()->OfferSave(profile, callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          testing::_));
  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

}  // namespace autofill
