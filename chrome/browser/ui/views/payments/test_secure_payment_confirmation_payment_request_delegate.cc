// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"

namespace payments {

TestSecurePaymentConfirmationPaymentRequestDelegate::
    TestSecurePaymentConfirmationPaymentRequestDelegate(
        content::WebContents* web_contents,
        base::WeakPtr<SecurePaymentConfirmationModel> model,
        SecurePaymentConfirmationDialogView::ObserverForTest* observer)
    : ChromePaymentRequestDelegate(web_contents),
      web_contents_(web_contents),
      model_(model),
      dialog_view_(
          (new SecurePaymentConfirmationDialogView(observer))->GetWeakPtr()) {}

TestSecurePaymentConfirmationPaymentRequestDelegate::
    ~TestSecurePaymentConfirmationPaymentRequestDelegate() = default;

void TestSecurePaymentConfirmationPaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  dialog_view_->ShowDialog(web_contents_, model_->GetWeakPtr(),
                           base::DoNothing(), base::DoNothing());
}

void TestSecurePaymentConfirmationPaymentRequestDelegate::CloseDialog() {
  dialog_view_->HideDialog();
}

}  // namespace payments
