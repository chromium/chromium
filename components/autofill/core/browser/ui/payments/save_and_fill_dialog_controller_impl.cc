// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveAndFillDialogControllerImpl::SaveAndFillDialogControllerImpl() = default;
SaveAndFillDialogControllerImpl::~SaveAndFillDialogControllerImpl() = default;

void SaveAndFillDialogControllerImpl::ShowLocalDialog(
    base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
        create_and_show_view_callback,
    payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback
        card_save_and_fill_dialog_callback) {
  dialog_state_ = SaveAndFillDialogState::kLocalDialog;
  dialog_view_ = std::move(create_and_show_view_callback).Run();
  card_save_and_fill_dialog_callback_ =
      std::move(card_save_and_fill_dialog_callback);
  CHECK(dialog_view_);
  autofill_metrics::LogSaveAndFillDialogShown(/*is_upload=*/false);
}

void SaveAndFillDialogControllerImpl::ShowUploadDialog(
    const LegalMessageLines& legal_message_lines,
    base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
        create_and_show_view_callback,
    payments::PaymentsAutofillClient::CardSaveAndFillDialogCallback
        card_save_and_fill_dialog_callback) {
  dialog_state_ = SaveAndFillDialogState::kUploadDialog;
  legal_message_lines_ = legal_message_lines;
  dialog_view_ = std::move(create_and_show_view_callback).Run();
  card_save_and_fill_dialog_callback_ =
      std::move(card_save_and_fill_dialog_callback);
  CHECK(dialog_view_);
  autofill_metrics::LogSaveAndFillDialogShown(/*is_upload=*/true);
}

void SaveAndFillDialogControllerImpl::ShowPendingDialog(
    base::OnceCallback<std::unique_ptr<SaveAndFillDialogView>()>
        create_and_show_view_callback) {
  dialog_state_ = SaveAndFillDialogState::kPendingDialog;
  dialog_view_ = std::move(create_and_show_view_callback).Run();
  CHECK(dialog_view_);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::u16string SaveAndFillDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_TITLE);
}

std::u16string SaveAndFillDialogControllerImpl::GetExplanatoryMessage() const {
  switch (dialog_state_) {
    case SaveAndFillDialogState::kUploadDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_UPLOAD);
    case SaveAndFillDialogState::kLocalDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPLANATION_LOCAL);
    case SaveAndFillDialogState::kPendingDialog:
      return std::u16string();
  }
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
SaveAndFillDialogControllerImpl::GetInvalidExpirationDateErrorMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_EXPIRATION_DATE);
}

std::u16string
SaveAndFillDialogControllerImpl::GetInvalidNameOnCardErrorMessage() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_INVALID_NAME_ON_CARD);
}

std::u16string SaveAndFillDialogControllerImpl::FormatExpirationDateInput(
    std::u16string_view input,
    size_t old_cursor_position,
    size_t& new_cursor_position) const {
  std::u16string cleaned_input;
  for (char16_t c : input) {
    if (base::IsAsciiDigit(c)) {
      cleaned_input += c;
    }
  }

  // Format input into `MM/YY` and restrict input to 5 characters.
  std::u16string formatted_input =
      cleaned_input.length() > 2
          ? cleaned_input.substr(0, 2) + u"/" + cleaned_input.substr(2, 2)
          : cleaned_input;

  new_cursor_position = old_cursor_position;

  if (input != formatted_input) {
    // Scenario A: A slash was just inserted.
    // This happens when the input has 2 characters (e.g.,`12`) and
    // `formatted_input` becomes 4 characters (e.g., `12/3`) because the user
    // typed a third digit.
    if (formatted_input.length() == input.length() + 2 &&
        old_cursor_position == 2) {
      new_cursor_position = old_cursor_position + 1;
    } else if (formatted_input.length() > input.length()) {
      // Scenario B: Text length grew due to adding more digits.
      new_cursor_position =
          old_cursor_position + (formatted_input.length() - input.length());
    } else {
      // Scenario C: Text length shrank due to deletion or remained the same.
      new_cursor_position = old_cursor_position;
    }
    // Ensure the calculated cursor position does not exceed the actual length
    // of the new `formatted_input`. This is crucial for handling deletions or
    // truncation, preventing out-of-bounds cursor placement.
    new_cursor_position =
        std::min(new_cursor_position, formatted_input.length());
  }

  return formatted_input;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

SaveAndFillDialogState SaveAndFillDialogControllerImpl::GetDialogState() const {
  return dialog_state_;
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

bool SaveAndFillDialogControllerImpl::IsValidExpirationDate(
    std::u16string_view expiration_date) const {
  if (expiration_date.empty() || expiration_date.length() < 5) {
    return false;
  }

  size_t slash_pos = expiration_date.find(u'/');
  if (slash_pos == std::u16string::npos) {
    return false;
  }

  std::u16string_view month = expiration_date.substr(0, slash_pos);
  std::u16string_view year = expiration_date.substr(slash_pos + 1);

  if (month.size() != 2U || year.size() != 2U) {
    return false;
  }

  int month_value, year_value = 0;
  if (!base::StringToInt(month, &month_value) ||
      !base::StringToInt(year, &year_value)) {
    return false;
  }

  return IsValidCreditCardExpirationDate(year_value, month_value,
                                         AutofillClock::Now());
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

const LegalMessageLines& SaveAndFillDialogControllerImpl::GetLegalMessageLines()
    const {
  return legal_message_lines_;
}

void SaveAndFillDialogControllerImpl::Dismiss() {
  dialog_view_.reset();
}

void SaveAndFillDialogControllerImpl::OnUserAcceptedDialog(
    const payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  autofill_metrics::LogSaveAndFillDialogResult(
      user_provided_card_save_and_fill_details.security_code.has_value()
          ? autofill_metrics::SaveAndFillDialogResult::kAcceptedWithCvc
          : autofill_metrics::SaveAndFillDialogResult::kAcceptedWithoutCvc);
  if (!card_save_and_fill_dialog_callback_.is_null()) {
    std::move(card_save_and_fill_dialog_callback_)
        .Run(payments::PaymentsAutofillClient::
                 CardSaveAndFillDialogUserDecision::kAccepted,
             user_provided_card_save_and_fill_details);
  }
}

void SaveAndFillDialogControllerImpl::OnUserCanceledDialog() {
  autofill_metrics::LogSaveAndFillDialogResult(
      autofill_metrics::SaveAndFillDialogResult::kCanceled);
  Dismiss();
  if (!card_save_and_fill_dialog_callback_.is_null()) {
    std::move(card_save_and_fill_dialog_callback_)
        .Run(payments::PaymentsAutofillClient::
                 CardSaveAndFillDialogUserDecision::kDeclined,
             /*user_provided_card_save_and_fill_details=*/{});
  }
}

base::WeakPtr<SaveAndFillDialogController>
SaveAndFillDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
