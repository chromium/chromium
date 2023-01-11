// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_VIEW_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class SecurePaymentConfirmationNoCredsModel;

// Draws the user interface in the payment credential enrollment no matching
// credentials flow.
class SecurePaymentConfirmationNoCredsView {
 public:
  using ResponseCallback = base::OnceClosure;
  using OptOutCallback = base::OnceClosure;

  static base::WeakPtr<SecurePaymentConfirmationNoCredsView> Create();

  virtual ~SecurePaymentConfirmationNoCredsView() = 0;

  virtual void ShowDialog(
      content::WebContents* web_contents,
      base::WeakPtr<SecurePaymentConfirmationNoCredsModel> model,
      ResponseCallback response_callback,
      OptOutCallback opt_out_callback) = 0;
  virtual void HideDialog() = 0;
  virtual bool ClickOptOutForTesting() = 0;

 protected:
  SecurePaymentConfirmationNoCredsView();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_VIEW_H_
