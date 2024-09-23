// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_UI_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_UI_INFO_H_

#include <string>

#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "ui/gfx/image/image.h"

struct AccountInfo;

namespace autofill {

class CreditCard;

// Holds resources (strings, icons) for save card prompt UIs.
struct AutofillSaveCardUiInfo {
  bool is_for_upload;
  // The resource ID for the logo displayed for the dialog.
  int logo_icon_id;
  // The resource ID for the icon that identifies the issuer of the card.
  int issuer_icon_id;
  LegalMessageLines legal_message_lines;
  std::u16string card_label;
  std::u16string card_sub_label;
  std::u16string card_last_four_digits;
  std::u16string cardholder_name;
  std::u16string expiration_date_month;
  std::u16string expiration_date_year;
  // Accessibility description for a card chip containing the card icon, label
  // and sub label.
  std::u16string card_description;
  std::u16string displayed_target_account_email;
  gfx::Image displayed_target_account_avatar;
  // Title of the UI displayed after the logo icon.
  std::u16string title_text;
  std::u16string confirm_text;
  std::u16string cancel_text;
  // Description text to be shown above the card information in the prompt.
  std::u16string description_text;
  // Accessibility description when a loading spinner is shown.
  std::u16string loading_description;
  bool is_google_pay_branding_enabled;

  AutofillSaveCardUiInfo();
  ~AutofillSaveCardUiInfo();

  // AutofillSaveCardUiInfo is not copyable.
  AutofillSaveCardUiInfo(const AutofillSaveCardUiInfo&) = delete;
  AutofillSaveCardUiInfo& operator=(const AutofillSaveCardUiInfo&) = delete;

  // AutofillSaveCardUiInfo is moveable.
  AutofillSaveCardUiInfo(AutofillSaveCardUiInfo&& other) noexcept;
  AutofillSaveCardUiInfo& operator=(AutofillSaveCardUiInfo&& other);

  // Create the ui info for a local save prompt.
  // Requires that `options.card_save_type` is not equal to
  // `payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly`
  static AutofillSaveCardUiInfo CreateForLocalSave(
      payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      const CreditCard& card);

  // Create the ui info for a server save prompt.
  static AutofillSaveCardUiInfo CreateForUploadSave(
      payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      const AccountInfo& displayed_target_account);

  // Create the ui info for a server save prompt.
  //
  // This function allows specifying whether google pay branding is enabled.
  // Requires `options.card_save_type` not equal to
  // `payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly`.
  static AutofillSaveCardUiInfo CreateForUploadSave(
      payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      const AccountInfo& displayed_target_account,
      bool is_google_pay_branding_enabled);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_UI_INFO_H_
