// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_strings.h"

namespace payments {
namespace errors {

// Please keep the list alphabetized.
// Each string must be on a single line to correctly generate ErrorStrings.java.

const char kAnotherUiShowing[] = "Another PaymentRequest UI is already showing in a different tab or window.";
const char kAttemptedInitializationTwice[] = "Attempted initialization twice.";
const char kCannotShowInBackgroundTab[] = "Cannot show PaymentRequest UI in a background tab.";
const char kCannotShowTwice[] = "Attempted show twice.";
const char kCannotShowWithoutInit[] = "Attempted show without initialization.";
const char kCannotUpdateWithoutInit[] = "Attempted updateWith without initialization.";
const char kCannotUpdateWithoutShow[] = "Attempted updateWith without show.";
const char kGenericPaymentMethodNotSupportedMessage[] = "Payment method not supported.";
const char kInvalidState[] = "Invalid state.";
const char kNotInASecureOrigin[] = "Not in a secure origin.";
const char kProhibitedOrigin[] = "Only localhost, file://, and cryptographic scheme origins allowed.";
const char kProhibitedOriginOrInvalidSslExplanation[] = "No UI will be shown. CanMakePayment and hasEnrolledInstrument will always return false. Show will be rejected with NotSupportedError.";
const char kStrictBasicCardShowReject[] = "User does not have valid information on file.";
const char kTotalRequired[] = "Total required.";
const char kUserCancelled[] = "User closed the Payment Request UI.";

}  // namespace errors
}  // namespace payments
