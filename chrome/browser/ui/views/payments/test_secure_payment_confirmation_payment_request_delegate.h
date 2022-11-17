// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_SECURE_PAYMENT_CONFIRMATION_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_SECURE_PAYMENT_CONFIRMATION_PAYMENT_REQUEST_DELEGATE_H_

#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {

class PaymentRequest;

// Implementation of the Secure Payment Confirmation delegate used in tests.
class TestSecurePaymentConfirmationPaymentRequestDelegate
    : public ChromePaymentRequestDelegate {
 public:
  // This delegate does not own things passed as pointers.
  TestSecurePaymentConfirmationPaymentRequestDelegate(
      content::RenderFrameHost* render_frame_host,
      base::WeakPtr<SecurePaymentConfirmationModel> model,
      base::WeakPtr<SecurePaymentConfirmationDialogView::ObserverForTest>
          observer);
  ~TestSecurePaymentConfirmationPaymentRequestDelegate() override;

  // ChromePaymentRequestDelegate:
  void ShowDialog(base::WeakPtr<PaymentRequest> request) override;
  void CloseDialog() override;

  SecurePaymentConfirmationDialogView* dialog_view() {
    return dialog_view_.get();
  }

 private:
  base::WeakPtr<SecurePaymentConfirmationModel> model_;
  base::WeakPtr<SecurePaymentConfirmationDialogView> dialog_view_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_SECURE_PAYMENT_CONFIRMATION_PAYMENT_REQUEST_DELEGATE_H_
