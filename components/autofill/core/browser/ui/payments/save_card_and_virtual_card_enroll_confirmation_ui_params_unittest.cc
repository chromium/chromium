// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_card_and_virtual_card_enroll_confirmation_ui_params.h"

#include <string>

#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// Verify that SaveCardAndVirtualCardEnrollConfirmationUiParams attributes
// are correctly set for save card upload success.
TEST(SaveCardAndVirtualCardEnrollConfirmationUiParamsTest,
     VerifyAttributesForSaveCardSuccess) {
  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params =
      SaveCardAndVirtualCardEnrollConfirmationUiParams::
          CreateForSaveCardSuccess();

  EXPECT_TRUE(ui_params.is_success);
  EXPECT_EQ(ui_params.title_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_EQ(ui_params.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_TRUE(ui_params.failure_ok_button_text.empty());
  EXPECT_TRUE(ui_params.failure_ok_button_accessible_name.empty());
}

// Verify that SaveCardAndVirtualCardEnrollConfirmationUiParams attributes
// are correctly set for virtual card enrollment success.
TEST(SaveCardAndVirtualCardEnrollConfirmationUiParamsTest,
     VerifyAttributesForVirtualCardSuccess) {
  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params =
      SaveCardAndVirtualCardEnrollConfirmationUiParams::
          CreateForVirtualCardSuccess();

  EXPECT_TRUE(ui_params.is_success);
  EXPECT_EQ(
      ui_params.title_text,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_EQ(
      ui_params.description_text,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_TRUE(ui_params.failure_ok_button_text.empty());
  EXPECT_TRUE(ui_params.failure_ok_button_accessible_name.empty());
}

// Verify that SaveCardAndVirtualCardEnrollConfirmationUiParams attributes
// are correctly set for save card upload failure.
TEST(SaveCardAndVirtualCardEnrollConfirmationUiParamsTest,
     VerifyAttributesForSaveCardFailure) {
  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params =
      SaveCardAndVirtualCardEnrollConfirmationUiParams::
          CreateForSaveCardFailure();

  EXPECT_FALSE(ui_params.is_success);
  EXPECT_EQ(ui_params.title_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_EQ(ui_params.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
  EXPECT_EQ(
      ui_params.failure_ok_button_text,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_OK_BUTTON_TEXT));
  EXPECT_EQ(
      ui_params.failure_ok_button_accessible_name,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_OK_BUTTON_ACCESSIBLE_NAME));
}

// Verify that SaveCardAndVirtualCardEnrollConfirmationUiParams attributes
// are correctly set for virtual card enrollment failure.
TEST(SaveCardAndVirtualCardEnrollConfirmationUiParamsTest,
     VerifyAttributesForVirtualCardFailure) {
  std::u16string card_label = u"Visa ****2345";
  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params =
      SaveCardAndVirtualCardEnrollConfirmationUiParams::
          CreateForVirtualCardFailure(card_label);

  EXPECT_FALSE(ui_params.is_success);
  EXPECT_EQ(
      ui_params.title_text,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_EQ(
      ui_params.description_text,
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_DESCRIPTION_TEXT,
          card_label));
  EXPECT_EQ(
      ui_params.failure_ok_button_text,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_OK_BUTTON_TEXT));
  EXPECT_EQ(
      ui_params.failure_ok_button_accessible_name,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_OK_BUTTON_ACCESSIBLE_NAME));
}

}  // namespace autofill
