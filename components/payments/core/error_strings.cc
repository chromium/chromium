// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_strings.h"

namespace payments {
namespace errors {

// Please keep the list alphabetized.
// Each string must be on a single line to correctly generate ErrorStrings.java.
// clang-format off
const char kAnotherUiShowing[] = "Another PaymentRequest UI is already showing in a different tab or window.";
const char kAppStoreMethodOnlySupportedInTwa[] = "Payment method https://play.google.com/billing is only supported in Trusted Web Activity.";
const char kAttemptedInitializationTwice[] = "Attempted initialization twice.";
const char kCannotShowInBackgroundTab[] = "Cannot show PaymentRequest UI in a preview page or a background tab.";
const char kCannotShowTwice[] = "Attempted show twice.";
const char kCannotShowWithoutInit[] = "Attempted show without initialization.";
const char kCannotUpdateWithoutInit[] = "Attempted updateWith without initialization.";
const char kCannotUpdateWithoutShow[] = "Attempted updateWith without show.";
const char kInvalidState[] = "Invalid state.";
const char kMethodDataRequired[] = "Method data required.";
const char kMethodNameRequired[] = "Method name required.";
const char kMissingDetailsFromPaymentApp[] = "Payment app returned invalid response. Missing field \"details\".";
const char kMissingMethodNameFromPaymentApp[] = "Payment app returned invalid response. Missing field \"methodName\".";
const char kNotInASecureOrigin[] = "Not in a secure origin.";
const char kNoWebContents[] = "The frame that initiated payment is not associated with any web page.";
const char kPayerEmailEmpty[] = "Payment app returned invalid response. Missing field \"payerEmail\".";
const char kPayerNameEmpty[] = "Payment app returned invalid response. Missing field \"payerName\".";
const char kPayerPhoneEmpty[] = "Payment app returned invalid response. Missing field \"payerPhone\".";
const char kProhibitedOrigin[] = "Only localhost, file://, and cryptographic scheme origins allowed.";
const char kProhibitedOriginOrInvalidSslExplanation[] = "No UI will be shown. CanMakePayment and hasEnrolledInstrument will always return false. Show will be rejected with NotSupportedError.";
const char kShippingAddressInvalid[] = "Payment app returned invalid shipping address in response.";
const char kShippingOptionEmpty[] = "Payment app returned invalid response. Missing field \"shipping option\".";
const char kShippingOptionIdRequired[] = "Shipping option identifier required.";
const char kSkipAppForPartialDelegation[] = "Skipping $ for not providing all of the requested PaymentOptions.";
const char kStrictBasicCardShowReject[] = "User does not have valid information on file.";
const char kTotalRequired[] = "Total required.";
const char kUserCancelled[] = "User closed the Payment Request UI.";
const char kWebAuthnOperationTimedOutOrNotAllowed[] = "The operation either timed out or was not allowed. See: https://www.w3.org/TR/webauthn-2/#sctn-privacy-considerations-client.";
const char kSpcUserOptedOut[] = "User opted out of the process.";
const char kInvalidPaymentDetails[] = "Invalid payment details.";
const char kInvalidPaymentOptions[] = "Invalid payment options.";
const char kCannotShowWithoutUserActivation[] = "PaymentRequest.show() calls after the first (per page load) require either transient user activation or delegated payment request capability.";
// clang-format on
}  // namespace errors
}  // namespace payments
