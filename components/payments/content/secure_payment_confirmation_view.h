// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VIEW_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentUIObserver;
class SecurePaymentConfirmationModel;

// Draws the user interface in the secure payment confirmation flow. Owned by
// the SecurePaymentConfirmationController.
class SecurePaymentConfirmationView {
 public:
  using VerifyCallback = base::OnceCallback<void()>;
  using CancelCallback = base::OnceCallback<void()>;
  using OptOutCallback = base::OnceCallback<void()>;

  static base::WeakPtr<SecurePaymentConfirmationView> Create(
      const base::WeakPtr<PaymentUIObserver> payment_ui_observer);

  virtual ~SecurePaymentConfirmationView() = 0;

  virtual void ShowDialog(content::WebContents* web_contents,
                          base::WeakPtr<SecurePaymentConfirmationModel> model,
                          VerifyCallback verify_callback,
                          CancelCallback cancel_callback,
                          OptOutCallback opt_out_callback) = 0;
  virtual void OnModelUpdated() = 0;
  virtual void HideDialog() = 0;
  virtual bool ClickOptOutForTesting() = 0;

 protected:
  SecurePaymentConfirmationView();

  base::WeakPtr<SecurePaymentConfirmationModel> model_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VIEW_H_
