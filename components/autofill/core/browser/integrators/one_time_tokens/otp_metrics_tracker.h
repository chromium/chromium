// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_constants.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace autofill {

class AutofillManager;

// Tab-scoped tracker that listens for OTP push notifications (tickles) to
// record metrics (e.g. arrival order, latency, form outcomes), even when OTP
// fields have not been detected yet.
class OtpMetricsTracker {
 public:
  static constexpr char kFieldDetectionToTickleLatencyHistogram[] =
      "Autofill.OneTimeTokens.FieldDetectionToTickleLatency";
  static constexpr char kTickleToFieldDetectionLatencyHistogram[] =
      "Autofill.OneTimeTokens.TickleToFieldDetectionLatency";
  static constexpr char kTickleFormOutcomeHistogram[] =
      "Autofill.OneTimeTokens.Tickle.FormOutcome";

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

  // Called when an OTP field is detected in a form. `autofill_manager` owns the
  // OTP form and is used to lazily evaluate user interaction with `field_ids`
  // in `form_id` upon tickle arrival.
  void OnOtpFieldDetected(FormGlobalId form_id,
                          std::vector<FieldGlobalId> field_ids,
                          base::WeakPtr<AutofillManager> autofill_manager);

#if defined(UNIT_TEST)
  // Returns true if there is an active tickle subscription.
  bool HasActiveSubscriptionForTesting() const {
    return tickle_subscription_.IsAlive();
  }
#endif

 private:
  void OnTickleReceived(one_time_tokens::OneTimeTokenSource source);
  void OnTickleTimeout();
  void OnFormOutcomeTimeout();
  bool IsOtpFieldEmptyAndUnedited() const;
  void ResetPendingFormState();

  raw_ptr<one_time_tokens::OneTimeTokenService> one_time_token_service_;
  one_time_tokens::ExpiringSubscription tickle_subscription_;

  // Reference to the `AutofillManager` that owns the OTP form. Used to lazily
  // query if the user typed in the field upon tickle arrival.
  base::WeakPtr<AutofillManager> autofill_manager_;

  // Global ID of the detected OTP form.
  std::optional<FormGlobalId> form_id_;

  // Global ID of the most recently handled OTP form whose outcome was recorded.
  // Used to prevent re-recording metrics if the same form is re-parsed.
  std::optional<FormGlobalId> last_handled_form_id_;

  // Global IDs of the detected OTP fields in `form_id_`.
  std::vector<FieldGlobalId> field_ids_;

  // Timestamp of the most recently detected OTP field. `std::nullopt` before
  // any OTP field is detected or once a session has completed/timed out.
  std::optional<base::TimeTicks> field_detection_time_;

  // Timestamp of the most recently received tickle.
  std::optional<base::TimeTicks> tickle_time_;

  // Timer to record `TickleArrival::kWithoutFieldDetection` when a tickle is
  // not followed by an OTP field detection within
  // `kNotificationExpirationDuration`.
  base::OneShotTimer tickle_timeout_timer_;

  // Timer to record `TickleFormOutcome::kNoTickleReceived` if no tickle arrives
  // within `kNotificationExpirationDuration` after an OTP field is detected.
  base::OneShotTimer form_outcome_timeout_timer_;

  base::WeakPtrFactory<OtpMetricsTracker> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
