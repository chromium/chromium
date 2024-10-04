// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_

#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/payments/content/payment_request_display_manager.h"

namespace payments {

// Used to interact with a cross-platform Payment Request dialog.
class PaymentRequestDialog {
 public:
  virtual ~PaymentRequestDialog() = default;

  virtual void ShowDialog() = 0;

  virtual void RetryDialog() = 0;

  virtual void CloseDialog() = 0;

  virtual void ShowErrorMessage() = 0;

  // Shows a "Processing..." spinner.
  virtual void ShowProcessingSpinner() = 0;

  // Whether a "Processing..." spinner is showing.
  virtual bool IsInteractive() const = 0;

  // Display |url| in a new screen and run |callback| after navigation is
  // completed, passing true/false to indicate success/failure.
  virtual void ShowPaymentHandlerScreen(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) = 0;

  // Confirms payment. Used only in tests.
  virtual void ConfirmPaymentForTesting() = 0;

  // Clicks the opt-out link. Returns true if the link was visible to the user,
  // false otherwise. Used only in tests.
  virtual bool ClickOptOutForTesting() = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_
