// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_

#include <string>

namespace autofill {

// The UI params for both the Save Card/IBAN and Virtual Card Enrollment
// confirmation dialogs. Since both dialogs have the same basic structure
// (title, description, and success/failure state), they share UI params to
// avoid code duplication.
struct SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams {
  ~SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams();

  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&);
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams& operator=(
      const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&);

  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&&);
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams& operator=(
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&&);

  // Static creator methods for the UI params in its various supported states.
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveCardSuccess();
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForVirtualCardSuccess();
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveCardFailure();
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForVirtualCardFailure(const std::u16string card_label);
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveIbanSuccess();
  static SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  CreateForSaveIbanFailure(bool hit_max_strikes);

  bool is_success;
  std::u16string title_text;
  std::u16string description_text;
  std::u16string failure_ok_button_text;
  std::u16string failure_ok_button_accessible_name;

 private:
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams(
      bool is_success,
      std::u16string title_text,
      std::u16string description_text,
      std::u16string failure_ok_button_accessible_name);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_UI_PARAMS_H_
