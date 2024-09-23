// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_

#include "base/types/expected.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}

namespace payments::facilitated {

// Reasons for why the payment was not offered. These only include the reasons
// after the renderer has detected a valid code and sent the signal to the
// browser process.
enum class PaymentNotOfferedReason {
  kApiNotAvailable = 0,
  kRiskDataEmpty = 1,
  kCodeValidatorFailed = 2,
  kInvalidCode = 3,
  kLandscapeScreenOrientation = 4,
  kMaxValue = kLandscapeScreenOrientation
};

// Result of the transaction from the time payment was offered to the user.
enum class TransactionResult {
  kFailed = 0,
  kSuccess = 1,
  kAbandoned = 2,
  kMaxValue = kAbandoned
};

// The trigger source for the facilitated payments transaction.
enum class TriggerSource {
  kUnknown = 0,
  kDOMSearch = 1,
  kCopyEvent = 2,
  kMaxValue = kCopyEvent
};

// Log the result and latency for validating a payment code using
// `data_decoder::DataDecoder`.
void LogPaymentCodeValidationResultAndLatency(
    base::expected<bool, std::string> result,
    base::TimeDelta duration);

// Log the result of whether the facilitated payments is available or not.
void LogIsApiAvailableResult(bool result, base::TimeDelta duration);

// Logs the result and latency for fetching the risk data. If the risk data was
// fetched successfully, `was_successful` is true. The call took `duration` to
// complete.
void LogLoadRiskDataResultAndLatency(bool was_successful,
                                     base::TimeDelta duration);

// Log the result of the GetClientToken call made to api client.
void LogGetClientTokenResult(bool result, base::TimeDelta duration);

// Log the reason for the payment option not offered to the user. This includes
// all the reasons after receiving a signal from the renderer process that a
// valid code has been found.
void LogPaymentNotOfferedReason(PaymentNotOfferedReason reason);

// Log the result and latency for the InitiatePayment backend endpoint.
void LogInitiatePaymentResult(bool result, base::TimeDelta duration);

// Log the result and latency for the InitiatePurchaseAction call made to the
// payments platform (client).
void LogInitiatePurchaseActionResult(bool result, base::TimeDelta duration);

// Log whether the request to show the FOP(form of payment) selector is
// successful or not.
void LogFopSelectorShown(bool shown);

// Log the overall transaction result. The transactions is considered to have
// started from the time payment was offered to the user.
void LogTransactionResult(TransactionResult result,
                          TriggerSource trigger_source,
                          base::TimeDelta duration,
                          ukm::SourceId ukm_source_id);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
