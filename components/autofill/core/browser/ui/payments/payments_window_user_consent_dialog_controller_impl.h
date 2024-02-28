// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_IMPL_H_

#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller.h"

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"

namespace autofill::payments {

class PaymentsWindowUserConsentDialogControllerImpl
    : public PaymentsWindowUserConsentDialogController {
 public:
  explicit PaymentsWindowUserConsentDialogControllerImpl(
      base::OnceClosure accept_callback);
  PaymentsWindowUserConsentDialogControllerImpl(
      const PaymentsWindowUserConsentDialogControllerImpl&) = delete;
  PaymentsWindowUserConsentDialogControllerImpl& operator=(
      const PaymentsWindowUserConsentDialogControllerImpl&) = delete;
  ~PaymentsWindowUserConsentDialogControllerImpl() override;

  // PaymentsWindowUserConsentDialogController:
  void OnOkButtonClicked() override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetDialogDescription() const override;
  std::u16string GetOkButtonLabel() const override;

 private:
  // Dialog acceptance callback. Set when `ShowDialog()` is triggered, and run
  // when `OnOkButtonClicked()` is called.
  base::OnceClosure accept_callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_IMPL_H_
