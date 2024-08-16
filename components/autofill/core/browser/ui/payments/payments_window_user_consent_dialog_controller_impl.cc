// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller_impl.h"

#include "components/autofill/core/browser/metrics/payments/payments_window_metrics.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

PaymentsWindowUserConsentDialogControllerImpl::
    PaymentsWindowUserConsentDialogControllerImpl(
        base::OnceClosure accept_callback,
        base::OnceClosure cancel_callback)
    : accept_callback_(std::move(accept_callback)),
      cancel_callback_(std::move(cancel_callback)) {}

PaymentsWindowUserConsentDialogControllerImpl::
    ~PaymentsWindowUserConsentDialogControllerImpl() {
  if (payments_window_user_consent_dialog_) {
    payments_window_user_consent_dialog_->Dismiss();
  }
}

void PaymentsWindowUserConsentDialogControllerImpl::ShowDialog(
    base::OnceCallback<base::WeakPtr<PaymentsWindowUserConsentDialog>(
        base::OnceClosure,
        base::OnceClosure)> create_and_show_dialog_callback) {
  CHECK(!payments_window_user_consent_dialog_);
  payments_window_user_consent_dialog_ =
      std::move(create_and_show_dialog_callback)
          .Run(base::BindOnce(&PaymentsWindowUserConsentDialogControllerImpl::
                                  OnOkButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()),
               base::BindOnce(&PaymentsWindowUserConsentDialogControllerImpl::
                                  OnCancelButtonClicked,
                              weak_ptr_factory_.GetWeakPtr()));
  CHECK(payments_window_user_consent_dialog_);
}

void PaymentsWindowUserConsentDialogControllerImpl::OnOkButtonClicked() {
  CHECK(accept_callback_);
  std::move(accept_callback_).Run();
}

void PaymentsWindowUserConsentDialogControllerImpl::OnCancelButtonClicked() {
  CHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}

void PaymentsWindowUserConsentDialogControllerImpl::OnDialogClosing(
    autofill_metrics::PaymentsWindowUserConsentDialogResult result) {
  autofill_metrics::LogPaymentsWindowUserConsentDialogResult(result);
}

std::u16string PaymentsWindowUserConsentDialogControllerImpl::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_LOADING_AND_CONSENT_DIALOG_TITLE_VCN_3DS);
}

std::u16string
PaymentsWindowUserConsentDialogControllerImpl::GetDialogDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_DESCRIPTION_VCN_3DS);
}

std::u16string PaymentsWindowUserConsentDialogControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_OK_BUTTON_LABEL_VCN_3DS);
}

}  // namespace autofill::payments
