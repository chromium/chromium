// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include <optional>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
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
  }

 protected:
  void OfferEditDefault() {
    controller()->OfferEdit(*profile_,
                            /*title_override=*/u"",
                            /*footer_message=*/u"",
                            /*is_editing_existing_address=*/false,
                            /*is_migration_to_account=*/false,
                            save_callback_.Get());
  }

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
  OfferEditDefault();
  EXPECT_CALL(save_callback_, Run).Times(0);
  DeleteContents();
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       IgnoreDialog_CancelCallbackInvoked) {
  OfferEditDefault();
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                  Property(&profile_ref::has_value, false)));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kIgnored, std::nullopt);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       CancelEditing_CancelCallbackInvoked) {
  OfferEditDefault();
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditDeclined,
                  Property(&profile_ref::has_value, false)));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditDeclined, std::nullopt);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       SaveAddress_SaveCallbackInvoked) {
  OfferEditDefault();
  EXPECT_CALL(save_callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                  AllOf(Property(&profile_ref::has_value, true),
                        Property(&profile_ref::value, *profile_))));

  controller()->OnDialogClosed(
      AutofillClient::AddressPromptUserDecision::kEditAccepted, *profile_);
}

TEST_F(EditAddressProfileDialogControllerImplTest,
       WindowTitleOverride_TitleUpdatedWhenParamIsPresent) {
  OfferEditDefault();
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

// Tests that offering a second edit prompt while one is active automatically
// autodeclines the second request.
TEST_F(EditAddressProfileDialogControllerImplTest,
       OfferEditTwice_SecondDialogAutodeclined) {
  OfferEditDefault();

  base::MockOnceCallback<void(AutofillClient::AddressPromptUserDecision,
                              profile_ref profile)>
      second_save_callback;
  EXPECT_CALL(second_save_callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&profile_ref::has_value, true)));

  controller()->OfferEdit(*profile_,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/false,
                          second_save_callback.Get());
}

// Tests that the positive button label returns "Save" when offering to edit a
// new address.
TEST_F(EditAddressProfileDialogControllerImplTest,
       PositiveButtonLabel_NewAddress_ReturnsSave) {
  controller()->OfferEdit(*profile_,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());

  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE));
}

// Tests that the positive button label returns "Update" when editing an
// existing address.
TEST_F(EditAddressProfileDialogControllerImplTest,
       PositiveButtonLabel_ExistingAddress_ReturnsUpdate) {
  controller()->OfferEdit(*profile_,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/true,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());

  EXPECT_EQ(controller()->GetOkButtonLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_UPDATE));
}

// Tests that local profiles are not validatable.
TEST_F(EditAddressProfileDialogControllerImplTest,
       GetIsValidatable_LocalProfile_ReturnsFalse) {
  controller()->OfferEdit(*profile_,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());
  EXPECT_FALSE(controller()->GetIsValidatable());
}

// Tests that account migration prompts are validatable.
TEST_F(EditAddressProfileDialogControllerImplTest,
       GetIsValidatable_AccountMigration_ReturnsTrue) {
  controller()->OfferEdit(*profile_,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/true,
                          save_callback_.Get());
  EXPECT_TRUE(controller()->GetIsValidatable());
}

// Tests that account profiles are validatable.
TEST_F(EditAddressProfileDialogControllerImplTest,
       GetIsValidatable_AccountProfile_ReturnsTrue) {
  AutofillProfile account_profile(AutofillProfile::RecordType::kAccount,
                                  AddressCountryCode("US"));
  controller()->OfferEdit(account_profile,
                          /*title_override=*/u"",
                          /*footer_message=*/u"",
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());
  EXPECT_TRUE(controller()->GetIsValidatable());
}

// Tests that the controller correctly initializes and returns profile and
// footer message getters.
TEST_F(EditAddressProfileDialogControllerImplTest,
       ControllerGettersInitialization) {
  AutofillProfile custom_profile(AddressCountryCode("US"));
  custom_profile.SetInfo(NAME_FULL, u"Jane Doe", "en-US");
  const std::u16string custom_footer = u"Custom footer message";

  controller()->OfferEdit(custom_profile,
                          /*title_override=*/u"", custom_footer,
                          /*is_editing_existing_address=*/false,
                          /*is_migration_to_account=*/false,
                          save_callback_.Get());

  EXPECT_EQ(controller()->GetProfileToEdit(), custom_profile);
  EXPECT_EQ(controller()->GetFooterMessage(), custom_footer);
}

}  // namespace autofill
