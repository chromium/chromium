// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class SaveAndFillDialogControllerImplTest : public testing::Test {
 protected:
  SaveAndFillDialogControllerImplTest() = default;
  ~SaveAndFillDialogControllerImplTest() override = default;

  void SetIsUploadSaveAndFill(bool is_upload) {
    controller_->is_upload_save_and_fill_ = is_upload;
  }

  void SetUp() override {
    controller_ = std::make_unique<SaveAndFillDialogControllerImpl>();

    EXPECT_CALL(create_and_show_view_callback, Run())
        .WillOnce(testing::Return(std::make_unique<SaveAndFillDialogView>()));

    controller_->ShowDialog(create_and_show_view_callback.Get());
  }

  SaveAndFillDialogControllerImpl* controller() const {
    return controller_.get();
  }

 private:
  std::unique_ptr<SaveAndFillDialogControllerImpl> controller_;
  base::MockCallback<
      base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>>
      create_and_show_view_callback;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(SaveAndFillDialogControllerImplTest, CorrectStringsAreReturned) {
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_TITLE));

  SetIsUploadSaveAndFill(false);
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_LOCAL));

  EXPECT_EQ(controller()->GetCardNumberLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CARD_NUMBER_LABEL));

  EXPECT_EQ(
      controller()->GetCvcLabel(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CVC_LABEL));

  EXPECT_EQ(controller()->GetExpirationDateLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPIRATION_DATE_LABEL));

  EXPECT_EQ(controller()->GetNameOnCardLabel(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_NAME_ON_CARD_LABEL));

  EXPECT_EQ(
      controller()->GetAcceptButtonText(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_ACCEPT));

  EXPECT_EQ(controller()->GetInvalidCardNumberErrorMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_CARD_NUMBER));

  // Test for upload Save and Fill explanatory message.
  SetIsUploadSaveAndFill(true);
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_UPLOAD));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(SaveAndFillDialogControllerImplTest, IsValidCvc) {
  // Empty CVC is valid since it's optional.
  EXPECT_TRUE(controller()->IsValidCvc(u""));

  // Valid 3-digit CVC.
  EXPECT_TRUE(controller()->IsValidCvc(u"123"));

  // Valid 4-digit CVC.
  EXPECT_TRUE(controller()->IsValidCvc(u"1234"));

  // CVC with less than 3 digits is invalid.
  EXPECT_FALSE(controller()->IsValidCvc(u"12"));
  EXPECT_FALSE(controller()->IsValidCvc(u"1"));

  // CVC with more than 4 digits is invalid.
  EXPECT_FALSE(controller()->IsValidCvc(u"12345"));

  // CVC with non-digit characters is invalid.
  EXPECT_FALSE(controller()->IsValidCvc(u"12A"));
  EXPECT_FALSE(controller()->IsValidCvc(u"ABC"));
  EXPECT_FALSE(controller()->IsValidCvc(u"1 3"));
}

TEST_F(SaveAndFillDialogControllerImplTest, IsValidNameOnCard) {
  EXPECT_TRUE(controller()->IsValidNameOnCard(u"John Doe"));
  EXPECT_TRUE(controller()->IsValidNameOnCard(u"Jane A. Smith-Jones"));
  EXPECT_TRUE(controller()->IsValidNameOnCard(u"O'Malley"));

  // The name on card field is required for this flow so an empty name is
  // considered invalid.
  EXPECT_FALSE(controller()->IsValidNameOnCard(u""));
  EXPECT_FALSE(controller()->IsValidNameOnCard(u"John123"));
  EXPECT_FALSE(controller()->IsValidNameOnCard(u"Invalid@Name"));
  EXPECT_FALSE(
      controller()->IsValidNameOnCard(u"This name is way too long for a card"));
}

}  // namespace autofill
