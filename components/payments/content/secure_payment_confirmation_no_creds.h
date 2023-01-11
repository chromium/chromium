// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/secure_payment_confirmation_no_creds_model.h"

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
  using OptOutCallback = base::OnceClosure;

  SecurePaymentConfirmationNoCreds();
  ~SecurePaymentConfirmationNoCreds();

  SecurePaymentConfirmationNoCreds(
      const SecurePaymentConfirmationNoCreds& other) = delete;
  SecurePaymentConfirmationNoCreds& operator=(
      const SecurePaymentConfirmationNoCreds& other) = delete;

  static std::unique_ptr<SecurePaymentConfirmationNoCreds> Create();

  void ShowDialog(content::WebContents* web_contents,
                  const std::u16string& merchant_name,
                  const std::string& rp_id,
                  ResponseCallback response_callback,
                  OptOutCallback opt_out_callback);
  void CloseDialog();
  bool ClickOptOutForTesting();

 private:
  // On desktop, the SecurePaymentConfirmationNoCredsView object is memory
  // managed by the views:: machinery. It is deleted when the window is closed
  // and views::DialogDelegateView::DeleteDelegate() is called by its
  // corresponding views::Widget.
  base::WeakPtr<SecurePaymentConfirmationNoCredsView> view_;

  SecurePaymentConfirmationNoCredsModel model_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_H_
