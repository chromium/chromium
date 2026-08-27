// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace autofill {

// Tab-scoped tracker that listens for OTP push notifications (tickles) to
// record metrics (e.g. arrival order, latency, form outcomes), even when OTP
// fields have not been detected yet.
class OtpMetricsTracker {
 public:
  explicit OtpMetricsTracker(
      one_time_tokens::OneTimeTokenService* one_time_token_service);
  OtpMetricsTracker(const OtpMetricsTracker&) = delete;
  OtpMetricsTracker& operator=(const OtpMetricsTracker&) = delete;
  virtual ~OtpMetricsTracker();

  // Called when an OTP field is detected in a form.
  void OnOtpFieldDetected();

#if defined(UNIT_TEST)
  // Returns true if there is an active tickle subscription.
  bool HasActiveSubscriptionForTesting() const {
    return tickle_subscription_.IsAlive();
  }
#endif

 private:
  void OnTickleReceived(one_time_tokens::OneTimeTokenSource source);

  raw_ptr<one_time_tokens::OneTimeTokenService> one_time_token_service_;
  one_time_tokens::ExpiringSubscription tickle_subscription_;

  base::WeakPtrFactory<OtpMetricsTracker> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
