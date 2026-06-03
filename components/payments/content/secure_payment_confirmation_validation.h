// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VALIDATION_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VALIDATION_H_

#include <string>

#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: (
//   SecurePaymentConfirmationRequestValidationError)
enum class SecurePaymentConfirmationRequestValidationError {
  kOk,
  kSPCMethodMustBeNull,
  kSPCMethodMustNotBeNull,
  kMultiplePaymentMethodsNotAllowed,
  kUnsupportedOptions,
  kCredentialIdsRequired,
  kTimeoutTooLong,
  kChallengeRequired,
  kInstrumentRequired,
  kInstrumentDisplayNameRequired,
  kValidInstrumentIconRequired,
  kNonUtf8InstrumentDetailsString,
  kEmptyInstrumentDetailsString,
  kTooLongInstrumentDetailsString,
  kRpIdRequired,
  kPayeeOriginOrPayeeNameRequired,
  kPayeeOriginMustBeHttps,
  kNonNullPaymentEntityLogoRequired,
  kValidLogoUrlRequired,
  kValidLogoUrlSchemeRequired,
  kLogoLabelRequired,
  kInternalError,
};

// Converts a SecurePaymentConfirmationRequestValidationError to a
// developer-facing string representation.
std::string SecurePaymentConfirmationRequestValidationErrorToString(
    SecurePaymentConfirmationRequestValidationError error);

// Validates the renderer-supplied `request` for a secure payment confirmation.
// Returns SecurePaymentConfirmationRequestValidationError::kOk if valid, or the
// specific validation error.
SecurePaymentConfirmationRequestValidationError
IsValidSecurePaymentConfirmationRequest(
    const mojom::SecurePaymentConfirmationRequestPtr& request);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VALIDATION_H_
