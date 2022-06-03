// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_VIEW_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_VIEW_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

// Draws the user interface in the payment credential enrollment no matching
// credentials flow.
class SecurePaymentConfirmationNoCredsView {
 public:
  using ResponseCallback = base::OnceClosure;

  static base::WeakPtr<SecurePaymentConfirmationNoCredsView> Create();

  virtual ~SecurePaymentConfirmationNoCredsView() = 0;

  virtual void ShowDialog(content::WebContents* web_contents,
                          const std::u16string& no_creds_text,
                          ResponseCallback response_callback) = 0;
  virtual void HideDialog() = 0;

 protected:
  SecurePaymentConfirmationNoCredsView();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_VIEW_H_
