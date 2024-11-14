// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_

#include "base/types/expected.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}

namespace payments::facilitated {

// Reasons for why the payflow was exited early. These only include the reasons
// after the renderer has detected a valid code and sent the signal to the
// browser process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PayflowExitedReason {
  // The code validator encountered an error.
  kCodeValidatorFailed = 0,
  // The code for the payflow is not valid.
  kInvalidCode = 1,
  // The user has opted out of the payflow.
  kUserOptedOut = 2,
  // The user has no linked accounts available for the payflow.
  kNoLinkedAccount = 3,
  // The device is in landscape orientation when payflow was to be triggered.
  kLandscapeScreenOrientation = 4,
  // The API Client is not available when the payflow was to be triggered.
  kApiClientNotAvailable = 5,
  // The risk data needed to send the server request is not available.
  kRiskDataNotAvailable = 6,
  // The client token needed to send the server request is not available.
  kClientTokenNotAvailable = 7,
  // The InitiatePayment response indicated a failure.
  kInitiatePaymentFailed = 8,
  // The action token returned in the InitiatePayment response is not available.
  kActionTokenNotAvailable = 9,
  // The user has logged out after selecting a payment method.
  kUserLoggedOut = 10,
  kMaxValue = kUserLoggedOut
};

// TODO(crbug.com/367751320): Remove after new PayflowExited histogram is
// finished.
// Reasons for why the payment was not offered. These only include the
// reasons after the renderer has detected a valid code and sent the signal to
// the browser process.
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

// Log when a Pix code is copied to the clippboard on an allowlisted merchant
// website.
void LogPixCodeCopied();

// Log when user selects a FOP to pay with.
void LogFopSelected();

// Log the result and latency for validating a payment code using
// `data_decoder::DataDecoder`.
void LogPaymentCodeValidationResultAndLatency(
    base::expected<bool, std::string> result,
    base::TimeDelta duration);

// Log the result of whether the facilitated payments is available or not and
// the check's latency.
void LogApiAvailabilityCheckResultAndLatency(bool result,
                                             base::TimeDelta duration);

// Logs the result and latency for fetching the risk data. If the risk data was
// fetched successfully, `was_successful` is true. The call took `duration` to
// complete.
void LogLoadRiskDataResultAndLatency(bool was_successful,
                                     base::TimeDelta duration);

// Log the result and the latency of the GetClientToken call made to api client.
void LogGetClientTokenResultAndLatency(bool result, base::TimeDelta duration);

// Log the reason for the payflow was exited early. This includes all the
// reasons after receiving a signal from the renderer process that a valid code
// has been found.
void LogPayflowExitedReason(PayflowExitedReason reason);

// TODO(crbug.com/367751320): Remove after new PayflowExited histogram is
// finished.
// Log the reason for the payment option not offered to the user. This
// includes all the reasons after receiving a signal from the renderer process
// that a valid code has been found.
void LogPaymentNotOfferedReason(PaymentNotOfferedReason reason);

// Log the attempt to send the call to the InitiatePayment backend endpoint.
void LogInitiatePaymentAttempt();

// Log the result and latency for the InitiatePayment backend endpoint.
void LogInitiatePaymentResultAndLatency(bool result, base::TimeDelta duration);

// Log the result and latency for the InitiatePurchaseAction call made to the
// payments platform (client).
void LogInitiatePurchaseActionResult(bool result, base::TimeDelta duration);

// Log whether the request to show the FOP(form of payment) selector is
// successful or not.
// TODO(crbug.com/377126728): Deprecate this method.
void LogFopSelectorShown(bool shown);

// Log the overall transaction result. The transactions is considered to have
// started from the time payment was offered to the user.
void LogTransactionResult(TransactionResult result,
                          TriggerSource trigger_source,
                          base::TimeDelta duration,
                          ukm::SourceId ukm_source_id);

// Logs showing a new UI screen.
void LogUiScreenShown(UiState ui_screen);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
