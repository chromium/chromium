// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller.h"

namespace autofill::payments {

class PaymentsWindowUserConsentDialog;

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

  // Shows the dialog, which is a view created by
  // `create_and_show_view_callback`. The controller contains the information
  // for what to show on the view, as well as the logic for handling user
  // interaction.
  void ShowDialog(
      base::OnceCallback<base::WeakPtr<PaymentsWindowUserConsentDialog>(
          base::OnceClosure)> create_and_show_dialog_callback);

  // PaymentsWindowUserConsentDialogController:
  void OnOkButtonClicked() override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetDialogDescription() const override;
  std::u16string GetOkButtonLabel() const override;

  base::WeakPtr<PaymentsWindowUserConsentDialogControllerImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Dialog acceptance callback. Set when `ShowDialog()` is triggered, and run
  // when `OnOkButtonClicked()` is called.
  base::OnceClosure accept_callback_;

  // The view that is currently displayed to the user, if one is present,
  // nullptr otherwise. Set when `ShowDialog()` is triggered, and reset on
  // controller or view destruction.
  base::WeakPtr<PaymentsWindowUserConsentDialog>
      payments_window_user_consent_dialog_;

  base::WeakPtrFactory<PaymentsWindowUserConsentDialogControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_IMPL_H_
