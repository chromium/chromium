// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller_impl.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

PaymentsWindowUserConsentDialogControllerImpl::
    PaymentsWindowUserConsentDialogControllerImpl(
        base::OnceClosure accept_callback)
    : accept_callback_(std::move(accept_callback)) {}

PaymentsWindowUserConsentDialogControllerImpl::
    ~PaymentsWindowUserConsentDialogControllerImpl() = default;

void PaymentsWindowUserConsentDialogControllerImpl::OnOkButtonClicked() {
  CHECK(accept_callback_);
  std::move(accept_callback_).Run();
}

std::u16string PaymentsWindowUserConsentDialogControllerImpl::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_TITLE_VCN_3DS);
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
