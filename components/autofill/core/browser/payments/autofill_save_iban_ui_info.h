// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_IBAN_UI_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_IBAN_UI_INFO_H_

#include <string>

#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {

// Holds resources for save IBAN bottom sheet UI.
struct AutofillSaveIbanUiInfo {
  // Initialized in CreateForUploadSave.
  int logo_icon_id;
  std::u16string iban_label;
  std::u16string title_text;
  // Initialized in CreateForUploadSave.
  std::u16string description_text;
  std::u16string accept_text;
  std::u16string cancel_text;
  // Initialized in CreateForUploadSave.
  LegalMessageLines legal_message_lines;

  AutofillSaveIbanUiInfo();

  // AutofillSaveIbanUiInfo is not copyable.
  AutofillSaveIbanUiInfo(const AutofillSaveIbanUiInfo&) = delete;
  AutofillSaveIbanUiInfo& operator=(const AutofillSaveIbanUiInfo&) = delete;

  // AutofillSaveIbanUiInfo is moveable.
  AutofillSaveIbanUiInfo(AutofillSaveIbanUiInfo&& other) noexcept;
  AutofillSaveIbanUiInfo& operator=(AutofillSaveIbanUiInfo&& other);

  ~AutofillSaveIbanUiInfo();

  // Create the ui info for a local save prompt.
  static AutofillSaveIbanUiInfo CreateForLocalSave(
      const std::u16string& iban_label);

  static AutofillSaveIbanUiInfo CreateForUploadSave(
      const std::u16string& iban_label,
      const LegalMessageLines& legal_message_lines);
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_IBAN_UI_INFO_H_
