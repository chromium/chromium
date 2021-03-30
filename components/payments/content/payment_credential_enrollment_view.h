// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_VIEW_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_VIEW_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentCredentialEnrollmentModel;

// Draws the user interface in the payment credential enrollment flow. Owned by
// the SecurePaymentConfirmationController.
class PaymentCredentialEnrollmentView {
 public:
  using AcceptCallback = base::OnceCallback<void()>;
  using CancelCallback = base::OnceCallback<void()>;

  static base::WeakPtr<PaymentCredentialEnrollmentView> Create();

  virtual ~PaymentCredentialEnrollmentView() = 0;

  virtual void ShowDialog(content::WebContents* web_contents,
                          base::WeakPtr<PaymentCredentialEnrollmentModel> model,
                          AcceptCallback accept_callback,
                          CancelCallback cancel_callback) = 0;
  virtual void OnModelUpdated() = 0;
  virtual void HideDialog() = 0;

 protected:
  PaymentCredentialEnrollmentView();

  base::WeakPtr<PaymentCredentialEnrollmentModel> model_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_VIEW_H_
