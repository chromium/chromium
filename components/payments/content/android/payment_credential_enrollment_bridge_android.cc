// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_credential_enrollment_bridge_android.h"
#include "content/public/browser/web_contents.h"

namespace payments {

// static
std::unique_ptr<PaymentCredentialEnrollmentBridge>
PaymentCredentialEnrollmentBridge::Create() {
  return std::make_unique<PaymentCredentialEnrollmentBridgeAndroid>();
}

void PaymentCredentialEnrollmentBridgeAndroid::ShowDialog(
    content::WebContents* web_contents,
    std::unique_ptr<SkBitmap> instrument_icon,
    const std::u16string& instrument_name,
    ResponseCallback response_callback) {
  // TODO(crbug.com/1204564): implement it.
}

void PaymentCredentialEnrollmentBridgeAndroid::ShowProcessingSpinner() {
  // TODO(crbug.com/1204564): implements it.
}

void PaymentCredentialEnrollmentBridgeAndroid::CloseDialog() {
  // TODO(crbug.com/1204564): implements it.
}

}  // namespace payments
