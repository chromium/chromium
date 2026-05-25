// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VALIDATION_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VALIDATION_H_

#include <string>

#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

// Validates the renderer-supplied `request` for a secure payment confirmation.
// Returns true if valid. If invalid, returns false and populates
// `error_message` with the validation error details.
bool IsValidSecurePaymentConfirmationRequest(
    const mojom::SecurePaymentConfirmationRequestPtr& request,
    std::string* error_message);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VALIDATION_H_
