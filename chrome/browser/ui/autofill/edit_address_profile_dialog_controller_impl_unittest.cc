// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include <optional>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::_;
using ::testing::AllOf;
using ::testing::NotNull;
using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

class EditAddressProfileDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  EditAddressProfileDialogControllerImplTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_.emplace(test::GetFullProfile());
    EditAddressProfileDialogControllerImpl::CreateForWebContents(
        web_contents());
    ASSERT_THAT(controller(), NotNull());
    controller()->SetViewFactoryForTest(base::BindRepeating(
        &EditAddressProfileDialogControllerImplTest::GetAutofillBubbleBase,
        base::Unretained(this)));
    controller()->OfferEdit(*profile_,
                            /*title_override=*/u"",
                            /*footer_message=*/u"",
                            /*is_editing_existing_address=*/false,
                            /*is_migration_to_account=*/false,
                            save_callback_.Get());
  }

 protected:
  std::unique_ptr<AutofillBubbleBase> GetAutofillBubbleBase(
      content::WebContents* web_contents,
      EditAddressProfileDialogController* controller) {
    return std::make_unique<TestAutofillBubble>();
  }

  EditAddressProfileDialogControllerImpl* controller() {
    return EditAddressProfileDialogControllerImpl::FromWebContents(
        web_contents());
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::optional<AutofillProfile> profile_;
  base::MockOnceCallback<void(AutofillClient::AddressPromptUserDecision,
                              profile_ref profile)>
      save_callback_;
};

TEST_F(EditAddressProfileDialogControllerImplTest,
       CloseTab_CallbackNotInvoked) {
  EXPECT_CALL(save_callback_, Run).Times(0);
  DeleteContents();
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
                        Property(&profile_ref::value, *profile_))));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditAccepted, *profile_);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       WindowTitleOverride_TitleUpdatedWhenParamIsPresent) {
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_TITLE));

  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditDeclined,
                  Property(&profile_ref::has_value, false)));
  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditDeclined, std::nullopt);

  base::MockOnceCallback<void(AutofillClient::AddressPromptUserDecision,
                              profile_ref profile)>
      second_save_callback;
  controller()->OfferEdit(*profile_, u"Overridden title",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/false,
                          second_save_callback.Get());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Overridden title");
}

}  // namespace autofill
