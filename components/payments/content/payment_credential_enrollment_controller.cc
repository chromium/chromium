// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_enrollment_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "build/build_config.h"
#include "components/payments/content/payment_credential_enrollment_model.h"

namespace payments {

PaymentCredentialEnrollmentController::PaymentCredentialEnrollmentController(
    content::WebContents* web_contents,
    AcceptCallback accept_callback,
    CancelCallback cancel_callback)
    : content::WebContentsObserver(web_contents),
      accept_callback_(std::move(accept_callback)),
      cancel_callback_(std::move(cancel_callback)) {}

PaymentCredentialEnrollmentController::
    ~PaymentCredentialEnrollmentController() = default;

void PaymentCredentialEnrollmentController::ShowDialog() {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif  // OS_ANDROID
  DCHECK(!view_);

  model_.set_progress_bar_visible(false);

  // TODO(crbug.com/1176368): Set dialog strings on the model.

  view_ =
      PaymentCredentialEnrollmentView::Create(/*payment_ui_observer=*/nullptr);
  view_->ShowDialog(
      web_contents(), model_.GetWeakPtr(),
      base::BindOnce(&PaymentCredentialEnrollmentController::OnConfirm,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PaymentCredentialEnrollmentController::OnCancel,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PaymentCredentialEnrollmentController::ShowProcessingSpinner() {
  if (!view_)
    return;

  model_.set_progress_bar_visible(true);
  model_.set_accept_button_enabled(false);
  model_.set_cancel_button_enabled(false);
  view_->OnModelUpdated();
}

void PaymentCredentialEnrollmentController::CloseDialog() {
  if (view_)
    view_->HideDialog();
}

void PaymentCredentialEnrollmentController::OnCancel() {
  CloseDialog();

  std::move(cancel_callback_).Run();
}

void PaymentCredentialEnrollmentController::OnConfirm() {
  DCHECK(web_contents());

  ShowProcessingSpinner();

  // This will trigger WebAuthn with OS-level UI (if any) on top of the |view_|
  // with its animated processing spinner. For example, on Linux, there's no
  // OS-level UI, while on MacOS, there's an OS-level prompt for the Touch ID
  // that shows on top of Chrome.
  std::move(accept_callback_).Run();
}

base::WeakPtr<PaymentCredentialEnrollmentController>
PaymentCredentialEnrollmentController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
