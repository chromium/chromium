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
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

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
    controller()->OfferEdit(profile_,
                            /*title_override=*/u"",
                            /*footer_message=*/u"",
                            /*is_editing_existing_address*/ false,
                            /*is_migration_to_account=*/false,
                            save_callback_.Get());
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
  base::MockOnceCallback<void(AutofillClient::AddressPromptUserDecision,
                              profile_ref profile)>
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
              Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                  Property(&profile_ref::has_value, false)));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kIgnored, std::nullopt);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       CancelEditing_CancelCallbackInvoked) {
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditDeclined,
                  Property(&profile_ref::has_value, false)));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditDeclined, std::nullopt);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       SaveAddress_SaveCallbackInvoked) {
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                  AllOf(Property(&profile_ref::has_value, true),
                        Property(&profile_ref::value, profile_))));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditAccepted, profile_);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       WindowTitleOverride_TitleUpdatedWhenParamIsPresent) {
  controller()->OfferEdit(profile_,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address*/ false,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_TITLE));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditDeclined, std::nullopt);
  controller()->OfferEdit(profile_, u"Overridden title",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address*/ false,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Overridden title");
}

}  // namespace autofill
