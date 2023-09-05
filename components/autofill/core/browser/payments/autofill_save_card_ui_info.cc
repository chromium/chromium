// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"

#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/grit/components_scaled_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillSaveCardUiInfo::AutofillSaveCardUiInfo() = default;
AutofillSaveCardUiInfo::~AutofillSaveCardUiInfo() = default;

AutofillSaveCardUiInfo::AutofillSaveCardUiInfo(
    AutofillSaveCardUiInfo&& other) noexcept = default;
AutofillSaveCardUiInfo& AutofillSaveCardUiInfo::operator=(
    AutofillSaveCardUiInfo&& other) = default;

static std::u16string GetConfirmButtonText(
    const AutofillClient::SaveCreditCardOptions& options) {
  // Requesting name or expiration date from the user makes the save prompt a
  // 2-step fix flow.
  bool prompt_continue = options.should_request_name_from_user ||
                         options.should_request_expiration_date_from_user;
  return l10n_util::GetStringUTF16(prompt_continue
                                       ? IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE
                                       : IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT);
}

static std::u16string GetCardDescription(
    const std::u16string& nickname,
    const std::u16string& network,
    const std::u16string& last_four_digits,
    const std::u16string& expiration_date) {
  if (nickname.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_CARD_DESCRIPTION, network,
        last_four_digits, expiration_date);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_CARD_DESCRIPTION_WITH_NICKNAME, nickname,
        network, last_four_digits, expiration_date);
  }
}

static AutofillSaveCardUiInfo CreateAutofillSaveCardUiInfo(
    bool is_for_upload,
    const CreditCard& card,
    int logo_icon_id,
    const LegalMessageLines& legal_message_lines,
    const AccountInfo& displayed_target_account,
    const std::u16string& title_text,
    const std::u16string& confirm_text,
    const std::u16string& cancel_text,
    const std::u16string& description_text,
    bool is_google_pay_branding_enabled) {
  AutofillSaveCardUiInfo ui_info;
  ui_info.is_for_upload = is_for_upload;
  ui_info.logo_icon_id = logo_icon_id;
  ui_info.issuer_icon_id = CreditCard::IconResourceId(card.network());
  ui_info.legal_message_lines = legal_message_lines;
  ui_info.card_label = card.CardNameAndLastFourDigits();
  ui_info.card_sub_label = card.AbbreviatedExpirationDateForDisplay(false);
  ui_info.card_last_four_digits = card.LastFourDigits();
  ui_info.cardholder_name = card.GetRawInfo(CREDIT_CARD_NAME_FULL);
  ui_info.expiration_date_month = card.Expiration2DigitMonthAsString();
  ui_info.expiration_date_year = card.Expiration4DigitYearAsString();
  ui_info.card_description = GetCardDescription(
      card.nickname(), card.NetworkForDisplay(), card.LastFourDigits(),
      card.ExpirationDateForDisplay());
  ui_info.displayed_target_account_email =
      base::UTF8ToUTF16((displayed_target_account.email));
  ui_info.displayed_target_account_avatar =
      displayed_target_account.account_image;
  ui_info.title_text = title_text;
  ui_info.confirm_text = confirm_text;
  ui_info.cancel_text = cancel_text;
  ui_info.description_text = description_text;
  ui_info.is_google_pay_branding_enabled = is_google_pay_branding_enabled;
  return ui_info;
}

// static
AutofillSaveCardUiInfo AutofillSaveCardUiInfo::CreateForLocalSave(
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card) {
  return CreateAutofillSaveCardUiInfo(
      /*is_for_upload=*/false, card, IDR_INFOBAR_AUTOFILL_CC,
      LegalMessageLines(), AccountInfo(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL),
      GetConfirmButtonText(options),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_THANKS_MOBILE_LOCAL_SAVE),
      /*description_text=*/base::EmptyString16(),
      /*is_google_pay_branding_enabled=*/false);
}

// static
AutofillSaveCardUiInfo AutofillSaveCardUiInfo::CreateForUploadSave(
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    const AccountInfo& displayed_target_account) {
  return AutofillSaveCardUiInfo::CreateForUploadSave(
      options, card, legal_message_lines, displayed_target_account,
      /*is_google_pay_branding_enabled=*/!!BUILDFLAG(GOOGLE_CHROME_BRANDING));
}

// static
AutofillSaveCardUiInfo AutofillSaveCardUiInfo::CreateForUploadSave(
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    const AccountInfo& displayed_target_account,
    bool is_google_pay_branding_enabled) {
  int save_card_icon_id = is_google_pay_branding_enabled
                              ? IDR_AUTOFILL_GOOGLE_PAY
                              : IDR_INFOBAR_AUTOFILL_CC;
  int save_card_prompt_title_id =
      is_google_pay_branding_enabled
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3
          : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD;
  std::u16string description_text =
      is_google_pay_branding_enabled
          ? l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3)
          : base::EmptyString16();
  return CreateAutofillSaveCardUiInfo(
      /*is_for_upload=*/true, card, save_card_icon_id, legal_message_lines,
      displayed_target_account,
      l10n_util::GetStringUTF16(save_card_prompt_title_id),
      GetConfirmButtonText(options),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_THANKS_MOBILE_UPLOAD_SAVE),
      description_text, is_google_pay_branding_enabled);
}

}  // namespace autofill
