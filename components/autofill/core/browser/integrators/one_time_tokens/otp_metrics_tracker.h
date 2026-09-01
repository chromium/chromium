// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_

#include <optional>

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
  static constexpr char kFieldDetectionToTickleLatencyHistogram[] =
      "Autofill.OneTimeTokens.FieldDetectionToTickleLatency";
  static constexpr char kTickleToFieldDetectionLatencyHistogram[] =
      "Autofill.OneTimeTokens.TickleToFieldDetectionLatency";

  // Maximum duration between OTP field detection and tickle arrival for them to
  // be considered correlated. If more than this time has passed, the tickle is
  // likely unrelated to the previously detected field, so the latency metric is
  // not recorded.
  static constexpr base::TimeDelta kFieldDetectionTimeout = base::Minutes(5);

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

  // Timestamp of the most recently detected OTP field. `std::nullopt` before
  // any OTP field is detected or once a session has completed/timed out.
  std::optional<base::TimeTicks> field_detection_time_;

  // Timestamp of the most recently received tickle.
  std::optional<base::TimeTicks> tickle_time_;

  base::WeakPtrFactory<OtpMetricsTracker> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
