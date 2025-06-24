// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveAndFillDialogControllerImpl::SaveAndFillDialogControllerImpl() = default;
SaveAndFillDialogControllerImpl::~SaveAndFillDialogControllerImpl() = default;

void SaveAndFillDialogControllerImpl::ShowDialog(
    base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
        create_and_show_view_callback) {
  dialog_view_ = std::move(create_and_show_view_callback).Run();
  CHECK(dialog_view_);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::u16string SaveAndFillDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_TITLE);
}

std::u16string SaveAndFillDialogControllerImpl::GetExplanatoryMessage() const {
  return l10n_util::GetStringUTF16(
      IsUploadSaveAndFill()
          ? IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_UPLOAD
          : IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_LOCAL);
}

std::u16string SaveAndFillDialogControllerImpl::GetCardNumberLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CARD_NUMBER_LABEL);
}

std::u16string SaveAndFillDialogControllerImpl::GetCvcLabel() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CVC_LABEL);
}

std::u16string SaveAndFillDialogControllerImpl::GetExpirationDateLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPIRATION_DATE_LABEL);
}

std::u16string SaveAndFillDialogControllerImpl::GetNameOnCardLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_NAME_ON_CARD_LABEL);
}

std::u16string SaveAndFillDialogControllerImpl::GetAcceptButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_ACCEPT);
}

std::u16string
SaveAndFillDialogControllerImpl::GetInvalidCardNumberErrorMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_CARD_NUMBER);
}

std::u16string SaveAndFillDialogControllerImpl::GetInvalidCvcErrorMessage()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_CVC);
}

std::u16string
SaveAndFillDialogControllerImpl::GetInvalidNameOnCardErrorMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_NAME_ON_CARD);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

bool SaveAndFillDialogControllerImpl::IsUploadSaveAndFill() const {
  return is_upload_save_and_fill_;
}

bool SaveAndFillDialogControllerImpl::IsValidCreditCardNumber(
    std::u16string_view input_text) const {
  return autofill::IsValidCreditCardNumber(input_text);
}

bool SaveAndFillDialogControllerImpl::IsValidCvc(
    std::u16string_view input_text) const {
  // If the CVC is empty, it's considered valid since it's an optional field.
  if (input_text.empty()) {
    return true;
  }

  // For non-empty CVC, it must be 3 or 4 digits.
  if (input_text.length() < 3 || input_text.length() > 4) {
    return false;
  }

  // Ensure all characters are digits.
  for (char16_t c : input_text) {
    if (!base::IsAsciiDigit(c)) {
      return false;
    }
  }
  return true;
}

bool SaveAndFillDialogControllerImpl::IsValidNameOnCard(
    std::u16string_view input_text) const {
  // The name on card field is normally optional for other card saving flows but
  // this flow requires a name on card to skip potential fix flows.
  if (input_text.empty()) {
    return false;
  }
  return autofill::IsValidNameOnCard(input_text);
}

base::WeakPtr<SaveAndFillDialogController>
SaveAndFillDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
