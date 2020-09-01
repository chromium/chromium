// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/payments/content/payment_request_display_manager.h"

namespace content {
class WebContents;
}

namespace payments {

// Used to interact with a cross-platform Payment Request dialog.
class PaymentRequestDialog {
 public:
  virtual ~PaymentRequestDialog() {}

  virtual void ShowDialog() = 0;

  virtual void RetryDialog() = 0;

  virtual void CloseDialog() = 0;

  virtual void ShowErrorMessage() = 0;

  // Shows a "Processing..." spinner.
  virtual void ShowProcessingSpinner() = 0;

  // Whether a "Processing..." spinner is showing.
  virtual bool IsInteractive() const = 0;

  // Shows the CVC unmask sheet and starts a FullCardRequest with the info
  // entered by the user.
  virtual void ShowCvcUnmaskPrompt(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate,
      content::WebContents* web_contents) = 0;

  // Display |url| in a new screen and run |callback| after navigation is
  // completed, passing true/false to indicate success/failure.
  virtual void ShowPaymentHandlerScreen(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) = 0;

  // Confirms payment. Used only in tests.
  virtual void ConfirmPaymentForTesting() = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_
