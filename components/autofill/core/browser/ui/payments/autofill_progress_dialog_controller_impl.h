// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_view.h"

namespace autofill {

enum class AutofillProgressDialogType;

// Implementation of the AutofillProgressDialogController. This class shows a
// progress bar with a cancel button that can be updated to a success state
// (check mark). The controller is destroyed once the view is dismissed.
class AutofillProgressDialogControllerImpl
    : public AutofillProgressDialogController {
 public:
  // The `autofill_progress_dialog_type` determines the type of the progress
  // dialog and `cancel_callback` is the function to invoke when the cancel
  // button is clicked.
  AutofillProgressDialogControllerImpl(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback);

  AutofillProgressDialogControllerImpl(
      const AutofillProgressDialogControllerImpl&) = delete;
  AutofillProgressDialogControllerImpl& operator=(
      const AutofillProgressDialogControllerImpl&) = delete;

  ~AutofillProgressDialogControllerImpl() override;

  // Show a progress dialog for underlying authorization processes. The
  // `create_and_show_view_callback` will be invoked immediately to create a
  // view implementation.
  void ShowDialog(
      base::OnceCallback<base::WeakPtr<AutofillProgressDialogView>()>
          create_and_show_view_callback);

  // Dismisses the progress dialog after the underlying authorization processes
  // have completed. If `show_confirmation_before_closing` is true, the UI
  // dismissal gets delayed and we show a confirmation screen to inform them
  // user that the authentication succeeded. The confirmation is automatically
  // dismissed after a short period of time and the progress dialog closes.
  //
  // It maybe be possible to authorize the filling without user interaction
  // (purely based on risk signals, the user did not had to type a password,
  // CVC, use biometrics, ...). If the authorization succeeded without user
  // interaction, DismissDialog calls `no_interactive_authentication_callback`
  // after closing the dialog.
  void DismissDialog(bool show_confirmation_before_closing,
                     base::OnceClosure no_interactive_authentication_callback =
                         base::OnceClosure());

  // AutofillProgressDialogController.
  void OnDismissed(bool is_canceled_by_user) override;
  std::u16string GetLoadingTitle() const override;
  std::u16string GetConfirmationTitle() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetLoadingMessage() const override;
  std::u16string GetConfirmationMessage() const override;
  base::WeakPtr<AutofillProgressDialogController> GetWeakPtr() override;

#if BUILDFLAG(IS_IOS)
  base::WeakPtr<AutofillProgressDialogControllerImpl> GetImplWeakPtr();
#endif

  base::WeakPtr<AutofillProgressDialogView> autofill_progress_dialog_view() {
    return autofill_progress_dialog_view_;
  }

 private:
  // View that displays the progress dialog.
  base::WeakPtr<AutofillProgressDialogView> autofill_progress_dialog_view_;

  // The type of the progress dialog that is being displayed.
  const AutofillProgressDialogType autofill_progress_dialog_type_;

  // Callback function invoked when the cancel button is clicked.
  base::OnceClosure cancel_callback_;

  base::OnceClosure no_interactive_authentication_callback_;

  base::WeakPtrFactory<AutofillProgressDialogControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_
