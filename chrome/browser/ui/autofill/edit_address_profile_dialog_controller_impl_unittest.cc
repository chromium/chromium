// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;

class EditAddressProfileDialogControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  EditAddressProfileDialogControllerImplTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    EditAddressProfileDialogControllerImpl::CreateForWebContents(
        web_contents());
    ASSERT_THAT(controller(), ::testing::NotNull());
    controller()->SetViewFactoryForTest(base::BindRepeating(
        &EditAddressProfileDialogControllerImplTest::GetAutofillBubbleBase,
        base::Unretained(this)));
    controller()->OfferEdit(profile_, /*original_profile=*/nullptr,
                            /*footer_message=*/u"", save_callback_.Get(),
                            /*is_migration_to_account=*/false);
  }

 protected:
  AutofillBubbleBase* GetAutofillBubbleBase(
      content::WebContents* web_contents,
      EditAddressProfileDialogController* controller) {
    return &autofill_bubble_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  EditAddressProfileDialogControllerImpl* controller() {
    return EditAddressProfileDialogControllerImpl::FromWebContents(
        web_contents());
  }

  AutofillProfile profile_ = test::GetFullProfile();
  TestAutofillBubble autofill_bubble_;
  base::MockOnceCallback<void(
      AutofillClient::SaveAddressProfileOfferUserDecision,
      AutofillProfile profile)>
      save_callback_;
};

TEST_F(EditAddressProfileDialogControllerImplTest,
       CloseTab_CancelCallbackInvoked) {
  EXPECT_CALL(save_callback_, Run).Times(0);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       IgnoreDialog_CancelCallbackInvoked) {
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));

  controller()->OnDialogClosed(
      AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored, profile_);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       CancelEditing_CancelCallbackInvoked) {
  EXPECT_CALL(
      save_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined,
          profile_));

  controller()->OnDialogClosed(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined,
      profile_);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       SaveAddress_SaveCallbackInvoked) {
  EXPECT_CALL(
      save_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
          profile_));

  controller()->OnDialogClosed(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
      profile_);
}

}  // namespace autofill
