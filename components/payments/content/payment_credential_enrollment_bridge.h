// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

// A platform-specific controller piece of the payment-credential-enrollment
// view. This header is implemented differently in Android and desktop, in
// order to allow different platforms with a different set of model and view,
// while reusing the same controller - PaymentCredentialEnrollmentController.
// Please try to reuse the cross-platform controller and minimize the
// platform-specific logic.
class PaymentCredentialEnrollmentBridge {
 public:
  virtual ~PaymentCredentialEnrollmentBridge() = default;

  PaymentCredentialEnrollmentBridge(
      const PaymentCredentialEnrollmentBridge& other) = delete;
  PaymentCredentialEnrollmentBridge& operator=(
      const PaymentCredentialEnrollmentBridge& other) = delete;

  using ResponseCallback = base::OnceCallback<void(bool user_accept_from_ui)>;

  // Create an instance of the bridge with a platform-specific implementation.
  static std::unique_ptr<PaymentCredentialEnrollmentBridge> Create();

  // Show the UI.
  // `web_contents`: The WebContents of the merchant.
  // `instrument_icon`: The icon of the payment instrument.
  // `instrument_name`: The name of the payment instrument.
  // `response_callback`: Invoked when the user responds to the UI.
  virtual void ShowDialog(content::WebContents* web_contents,
                          std::unique_ptr<SkBitmap> instrument_icon,
                          const std::u16string& instrument_name,
                          ResponseCallback response_callback) = 0;
  // Close the UI.
  virtual void CloseDialog() = 0;

  // Show processing spinner on the UI.
  virtual void ShowProcessingSpinner() = 0;

 protected:
  PaymentCredentialEnrollmentBridge() = default;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_BRIDGE_H_
