// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class SecurePaymentConfirmationNoCredsView;

// The desktop-specific part of the controller for the
// secure payment confirmation no matching creds UI.
class SecurePaymentConfirmationNoCreds {
 public:
  using ResponseCallback = base::OnceClosure;

  SecurePaymentConfirmationNoCreds();
  ~SecurePaymentConfirmationNoCreds();

  SecurePaymentConfirmationNoCreds(
      const SecurePaymentConfirmationNoCreds& other) = delete;
  SecurePaymentConfirmationNoCreds& operator=(
      const SecurePaymentConfirmationNoCreds& other) = delete;

  static std::unique_ptr<SecurePaymentConfirmationNoCreds> Create();

  void ShowDialog(content::WebContents* web_contents,
                  const std::u16string& merchant_name,
                  ResponseCallback response_callback);
  void CloseDialog();

 private:
  // On desktop, the SecurePaymentConfirmationNoCredsView object is memory
  // managed by the views:: machinery. It is deleted when the window is closed
  // and views::DialogDelegateView::DeleteDelegate() is called by its
  // corresponding views::Widget.
  base::WeakPtr<SecurePaymentConfirmationNoCredsView> view_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_H_
