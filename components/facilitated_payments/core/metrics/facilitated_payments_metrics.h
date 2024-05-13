// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_

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
  kMaxValue = kInvalidCode
};

// Log the result of whether the facilitated payments is available or not.
void LogIsApiAvailableResult(bool result, base::TimeDelta duration);

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

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_METRICS_FACILITATED_PAYMENTS_METRICS_H_
