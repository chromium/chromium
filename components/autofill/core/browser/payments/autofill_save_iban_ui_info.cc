// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_iban_ui_info.h"

#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillSaveIbanUiInfo::AutofillSaveIbanUiInfo() = default;

AutofillSaveIbanUiInfo::AutofillSaveIbanUiInfo(
    AutofillSaveIbanUiInfo&& other) noexcept = default;
AutofillSaveIbanUiInfo& AutofillSaveIbanUiInfo::operator=(
    AutofillSaveIbanUiInfo&& other) = default;

AutofillSaveIbanUiInfo::~AutofillSaveIbanUiInfo() = default;

static AutofillSaveIbanUiInfo CreateAutofillSaveIbanUiInfo(
    bool is_server_save,
    int logo_icon_id,
    const std::u16string& iban_value,
    const std::u16string& title_text,
    const std::u16string& description_text,
    const std::u16string& accept_text,
    const std::u16string& cancel_text,
    const LegalMessageLines& legal_message_lines) {
  AutofillSaveIbanUiInfo ui_info;
  ui_info.is_server_save = is_server_save;
  ui_info.logo_icon_id = logo_icon_id;
  ui_info.iban_value = iban_value;
  ui_info.title_text = title_text;
  ui_info.description_text = description_text;
  ui_info.accept_text = accept_text;
  ui_info.cancel_text = cancel_text;
  ui_info.legal_message_lines = legal_message_lines;

  return ui_info;
}

// static
AutofillSaveIbanUiInfo AutofillSaveIbanUiInfo::CreateForLocalSave(
    const std::u16string& iban_value) {
  return CreateAutofillSaveIbanUiInfo(
      /*is_server_save=*/false,
      /*logo_icon_id=*/0, iban_value,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL),
      /*description_text=*/std::u16string(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_ACCEPT),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_NO_THANKS),
      LegalMessageLines());
}

// static
AutofillSaveIbanUiInfo AutofillSaveIbanUiInfo::CreateForUploadSave(
    const std::u16string& iban_value,
    const LegalMessageLines& legal_message_lines) {
  return CreateAutofillSaveIbanUiInfo(
      /*is_server_save=*/true, IDR_AUTOFILL_GOOGLE_PAY, iban_value,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_SERVER),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_UPLOAD_IBAN_PROMPT_EXPLANATION),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_ACCEPT),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_NO_THANKS),
      legal_message_lines);
}

}  // namespace autofill
