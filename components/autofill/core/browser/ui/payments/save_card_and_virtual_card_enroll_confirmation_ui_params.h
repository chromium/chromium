// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_

#include <string>

namespace autofill {

// The UI params for both the Save Card and Virtual Card Enrollment confirmation
// dialogs. Since both dialogs have the same basic structure (title,
// description, and success/failure state), they share UI params to avoid code
// duplication.
struct SaveCardAndVirtualCardEnrollConfirmationUiParams {
  ~SaveCardAndVirtualCardEnrollConfirmationUiParams();

  SaveCardAndVirtualCardEnrollConfirmationUiParams(
      const SaveCardAndVirtualCardEnrollConfirmationUiParams&);
  SaveCardAndVirtualCardEnrollConfirmationUiParams& operator=(
      const SaveCardAndVirtualCardEnrollConfirmationUiParams&);

  SaveCardAndVirtualCardEnrollConfirmationUiParams(
      SaveCardAndVirtualCardEnrollConfirmationUiParams&&);
  SaveCardAndVirtualCardEnrollConfirmationUiParams& operator=(
      SaveCardAndVirtualCardEnrollConfirmationUiParams&&);

  // Static creator methods for the UI params in its various supported states.
  static SaveCardAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveCardSuccess();
  static SaveCardAndVirtualCardEnrollConfirmationUiParams
  CreateForVirtualCardSuccess();
  static SaveCardAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveCardFailure();
  static SaveCardAndVirtualCardEnrollConfirmationUiParams
  CreateForVirtualCardFailure(const std::u16string card_label);

  bool is_success;
  std::u16string title_text;
  std::u16string description_text;
  std::u16string failure_button_text;

 private:
  SaveCardAndVirtualCardEnrollConfirmationUiParams(
      bool is_success,
      std::u16string title_text,
      std::u16string description_text);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_
