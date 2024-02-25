// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_card_and_virtual_card_enroll_confirmation_ui_params.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardAndVirtualCardEnrollConfirmationUiParams::
    ~SaveCardAndVirtualCardEnrollConfirmationUiParams() = default;

SaveCardAndVirtualCardEnrollConfirmationUiParams::
    SaveCardAndVirtualCardEnrollConfirmationUiParams(
        const SaveCardAndVirtualCardEnrollConfirmationUiParams&) = default;
SaveCardAndVirtualCardEnrollConfirmationUiParams&
SaveCardAndVirtualCardEnrollConfirmationUiParams::operator=(
    const SaveCardAndVirtualCardEnrollConfirmationUiParams&) = default;

SaveCardAndVirtualCardEnrollConfirmationUiParams::
    SaveCardAndVirtualCardEnrollConfirmationUiParams(
        SaveCardAndVirtualCardEnrollConfirmationUiParams&&) = default;
SaveCardAndVirtualCardEnrollConfirmationUiParams&
SaveCardAndVirtualCardEnrollConfirmationUiParams::operator=(
    SaveCardAndVirtualCardEnrollConfirmationUiParams&&) = default;

SaveCardAndVirtualCardEnrollConfirmationUiParams::
    SaveCardAndVirtualCardEnrollConfirmationUiParams(
        bool is_success,
        std::u16string title_text,
        std::u16string description_text)
    : is_success(is_success),
      title_text(std::move(title_text)),
      description_text(std::move(description_text)),
      failure_button_text(
          is_success
              ? std::u16string()
              : l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_BUTTON_TEXT)) {
}

// static
SaveCardAndVirtualCardEnrollConfirmationUiParams
SaveCardAndVirtualCardEnrollConfirmationUiParams::CreateForSaveCardSuccess() {
  return SaveCardAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/true,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
}

// static
SaveCardAndVirtualCardEnrollConfirmationUiParams
SaveCardAndVirtualCardEnrollConfirmationUiParams::
    CreateForVirtualCardSuccess() {
  return SaveCardAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/true,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
}

// static
SaveCardAndVirtualCardEnrollConfirmationUiParams
SaveCardAndVirtualCardEnrollConfirmationUiParams::CreateForSaveCardFailure() {
  return SaveCardAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/false,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
}

// static
SaveCardAndVirtualCardEnrollConfirmationUiParams
SaveCardAndVirtualCardEnrollConfirmationUiParams::CreateForVirtualCardFailure(
    const std::u16string card_label) {
  return SaveCardAndVirtualCardEnrollConfirmationUiParams(
      /*is_success=*/false,
      /*title_text=*/
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_TITLE_TEXT),
      /*description_text=*/
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_DESCRIPTION_TEXT,
          card_label));
}

}  // namespace autofill
