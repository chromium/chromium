// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_FLOW_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_FLOW_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CvcFillingFlowType {
  // CVC filled without any interactive authentication.
  kNoInteractiveAuthentication = 0,
  // CVC filled with FIDO authentication.
  kFido = 1,
  // CVC filled with mandatory re-auth.
  kMandatoryReauth = 2,
  kMaxValue = kMandatoryReauth,
};

enum class ServerCardUnmaskFlowType {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Flow type was unknown at time of logging.
  kUnspecified = 0,
  // Only FIDO auth was offered.
  kFidoOnly = 1,
  // Only OTP auth was offered.
  kOtpOnly = 2,
  // FIDO auth was offered first but was cancelled or failed. OTP auth was
  // offered as a fallback.
  kOtpFallbackFromFido = 3,
  // Risk-based auth with no challenge involved.
  kRiskBased = 4,
  // Device unlock auth was offered.
  kDeviceUnlock = 5,
  // VCN 3DS auth was offered.
  kThreeDomainSecure = 6,
  kMaxValue = kThreeDomainSecure,
};

void LogServerCardUnmaskAttempt(
    payments::PaymentsAutofillClient::PaymentsRpcCardType card_type);
void LogCvcFilling(CvcFillingFlowType flow_type,
                   CreditCard::RecordType record_type);
void LogServerCardUnmaskResult(
    ServerCardUnmaskResult unmask_result,
    absl::variant<payments::PaymentsAutofillClient::PaymentsRpcCardType,
                  CreditCard::RecordType>,
    ServerCardUnmaskFlowType flow_type);
void LogServerCardUnmaskFormSubmission(
    payments::PaymentsAutofillClient::PaymentsRpcCardType card_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_FLOW_METRICS_H_
