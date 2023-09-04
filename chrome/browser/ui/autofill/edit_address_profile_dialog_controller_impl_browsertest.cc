// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;

class EditAddressProfileDialogControllerImplTest : public DialogBrowserTest {
 public:
  EditAddressProfileDialogControllerImplTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    EditAddressProfileDialogControllerImpl::CreateForWebContents(
        web_contents());
    EditAddressProfileDialogControllerImpl* dialog_controller = controller();
    ASSERT_THAT(dialog_controller, ::testing::NotNull());
    dialog_controller->OfferEdit(profile_, /*original_profile=*/nullptr,
                                 /*footer_message=*/u"", save_callback_.Get(),
                                 cancel_callback_.Get(),
                                 /*is_migration_to_account=*/false);
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  EditAddressProfileDialogControllerImpl* controller() {
    return EditAddressProfileDialogControllerImpl::FromWebContents(
        web_contents());
  }

  AutofillProfile profile_ = test::GetFullProfile();
  base::MockOnceCallback<void(
      AutofillClient::SaveAddressProfileOfferUserDecision,
      AutofillProfile profile)>
      save_callback_;
  base::MockOnceClosure cancel_callback_;
};

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       InvokeUi_Edit) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       CloseTab_CancelCallbackInvoked) {
  EXPECT_CALL(save_callback_, Run).Times(0);
  EXPECT_CALL(cancel_callback_, Run).Times(0);
  ShowUi("CloseTab_NoCallbacksInvoked");

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       IgnoreDialog_CancelCallbackInvoked) {
  EXPECT_CALL(save_callback_, Run).Times(0);
  EXPECT_CALL(cancel_callback_, Run);
  ShowUi("IgnoreDialog_CancelCallbackInvoked");

  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored, profile_);
}

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       CancelEditing_CancelCallbackInvoked) {
  EXPECT_CALL(save_callback_, Run).Times(0);
  EXPECT_CALL(cancel_callback_, Run);
  ShowUi("CancelEditing_CancelCallbackInvoked");

  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined,
      profile_);
}

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       SaveAddress_SaveCallbackInvoked) {
  EXPECT_CALL(
      save_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
          profile_));
  EXPECT_CALL(cancel_callback_, Run).Times(0);
  ShowUi("SaveAddress_SaveCallbackInvoked");

  controller()->OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
      profile_);
}

}  // namespace autofill
