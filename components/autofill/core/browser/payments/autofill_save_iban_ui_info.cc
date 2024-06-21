// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_iban_ui_info.h"

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
    const std::u16string& iban_label,
    const std::u16string& title_text,
    const std::u16string& accept_text,
    const std::u16string& cancel_text) {
  AutofillSaveIbanUiInfo ui_info;
  ui_info.iban_label = iban_label;
  ui_info.title_text = title_text;
  ui_info.accept_text = accept_text;
  ui_info.cancel_text = cancel_text;
  return ui_info;
}

// static
AutofillSaveIbanUiInfo AutofillSaveIbanUiInfo::CreateForLocalSave(
    const std::u16string& iban_label) {
  return CreateAutofillSaveIbanUiInfo(
      iban_label,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_TITLE_LOCAL),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_ACCEPT),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_MOBILE_NO_THANKS));
}

}  // namespace autofill
