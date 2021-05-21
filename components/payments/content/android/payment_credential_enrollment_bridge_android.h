// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_ANDROID_H_

#include "components/payments/content/payment_credential_enrollment_bridge.h"

namespace payments {

// The Android-specific part of the controller for the
// payment-credential-enrollment UI.
class PaymentCredentialEnrollmentBridgeAndroid
    : public PaymentCredentialEnrollmentBridge {
 public:
  PaymentCredentialEnrollmentBridgeAndroid() = default;
  ~PaymentCredentialEnrollmentBridgeAndroid() override = default;

  PaymentCredentialEnrollmentBridgeAndroid(
      const PaymentCredentialEnrollmentBridgeAndroid& other) = delete;
  PaymentCredentialEnrollmentBridgeAndroid& operator=(
      const PaymentCredentialEnrollmentBridgeAndroid& other) = delete;

  // PaymentCredentialEnrollmentBridge:
  void ShowDialog(content::WebContents* web_contents,
                  std::unique_ptr<SkBitmap> instrument_icon,
                  const std::u16string& instrument_name,
                  ResponseCallback response_callback) override;
  void CloseDialog() override;
  void ShowProcessingSpinner() override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_ANDROID_H_
