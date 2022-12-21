// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_FLOW_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_FLOW_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"

namespace autofill::autofill_metrics {

// All possible results of the card unmask flow.
enum class ServerCardUnmaskResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Default value, should never be used in logging.
  kUnknown = 0,
  // Card unmask completed successfully because the data had already been
  // cached locally.
  kLocalCacheHit = 1,
  // Card unmask completed successfully without further authentication steps.
  kRiskBasedUnmasked = 2,
  // Card unmask completed successfully via explicit authentication method,
  // such as FIDO, OTP, etc.
  kAuthenticationUnmasked = 3,
  // Card unmask failed due to some generic authentication errors.
  kAuthenticationError = 4,
  // Card unmask failed due to specific virtual card retrieval errors. Only
  // applies for virtual cards.
  kVirtualCardRetrievalError = 5,
  // Card unmask was aborted due to user cancellation.
  kFlowCancelled = 6,
  // Card unmask failed because only FIDO authentication was provided as an
  // option but the user has not opted in.
  kOnlyFidoAvailableButNotOptedIn = 7,
  // Card unmask failed due to unexpected errors.
  kUnexpectedError = 8,
  kMaxValue = kUnexpectedError,
};

// TODO(crbug.com/1263302): Right now this is only used for virtual cards.
// Extend it for masked server cards in the future too. Tracks the flow type
// used in a virtual card unmasking.
enum class VirtualCardUnmaskFlowType {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Flow type was not specified because of no option was provided, or was
  // unknown at time of logging.
  kUnspecified = 0,
  // Only FIDO auth was offered.
  kFidoOnly = 1,
  // Only OTP auth was offered.
  kOtpOnly = 2,
  // FIDO auth was offered first but was cancelled or failed. OTP auth was
  // offered as a fallback.
  kOtpFallbackFromFido = 3,
  kMaxValue = kOtpFallbackFromFido,
};

void LogServerCardUnmaskAttempt(AutofillClient::PaymentsRpcCardType card_type);

// TODO(crbug.com/1263302): These functions are used for only virtual cards
// now. Consider integrating with other masked server cards logging below.
void LogServerCardUnmaskResult(ServerCardUnmaskResult unmask_result,
                               AutofillClient::PaymentsRpcCardType card_type,
                               VirtualCardUnmaskFlowType flow_type);
void LogServerCardUnmaskFormSubmission(
    AutofillClient::PaymentsRpcCardType card_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_FLOW_METRICS_H_
