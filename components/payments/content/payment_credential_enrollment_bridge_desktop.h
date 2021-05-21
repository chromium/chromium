// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_DESKTOP_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_credential_enrollment_bridge.h"
#include "components/payments/content/payment_credential_enrollment_model.h"

namespace payments {

class PaymentCredentialEnrollmentView;

// The desktop-specific part of the controller for the
// payment-credential-enrollment UI.
class PaymentCredentialEnrollmentBridgeDesktop
    : public PaymentCredentialEnrollmentBridge {
 public:
  PaymentCredentialEnrollmentBridgeDesktop();
  ~PaymentCredentialEnrollmentBridgeDesktop() override;

  PaymentCredentialEnrollmentBridgeDesktop(
      const PaymentCredentialEnrollmentBridgeDesktop& other) = delete;
  PaymentCredentialEnrollmentBridgeDesktop& operator=(
      const PaymentCredentialEnrollmentBridgeDesktop& other) = delete;

  // PaymentCredentialEnrollmentBridge:
  void ShowDialog(content::WebContents* web_contents,
                  std::unique_ptr<SkBitmap> instrument_icon,
                  const std::u16string& instrument_name,
                  ResponseCallback response_callback) override;
  void CloseDialog() override;
  void ShowProcessingSpinner() override;

 private:
  PaymentCredentialEnrollmentModel model_;

  // On desktop, the PaymentCredentialEnrollmentView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  base::WeakPtr<PaymentCredentialEnrollmentView> view_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_DESKTOP_H_
