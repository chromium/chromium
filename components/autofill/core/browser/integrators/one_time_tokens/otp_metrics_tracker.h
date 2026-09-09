// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_METRICS_TRACKER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_constants.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace signin {
class IdentityManager;
}

namespace autofill {

class AutofillClient;
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
  static constexpr char kPageLanguageTickleBeforeUserInteractionHistogram[] =
      "Autofill.OneTimeTokens.PageLanguage.TickleBeforeUserInteraction";
  static constexpr char kPageLanguageTickleAfterUserInteractionHistogram[] =
      "Autofill.OneTimeTokens.PageLanguage.TickleAfterUserInteraction";
  static constexpr char kPageLanguageNoTickleReceivedHistogram[] =
      "Autofill.OneTimeTokens.PageLanguage.NoTickleReceived";
  static constexpr char kPageLanguageNoFieldDetectedHistogram[] =
      "Autofill.OneTimeTokens.PageLanguage.NoFieldDetected";

  // Maximum duration between OTP field detection and tickle arrival for them to
  // be considered correlated. If more than this time has passed, the tickle is
  // likely unrelated to the previously detected field, so the latency metric is
  // not recorded.
  static constexpr base::TimeDelta kFieldDetectionTimeout = base::Minutes(5);

  // Returns true if the user associated with `identity_manager` is eligible for
  // OTP metric tracking (signed into Chrome with a gmail.com or google.com
  // account).
  static bool IsEligibleForGmailOtps(
      const signin::IdentityManager* identity_manager);

  OtpMetricsTracker(
      one_time_tokens::OneTimeTokenService* one_time_token_service,
      AutofillClient& autofill_client);
  OtpMetricsTracker(const OtpMetricsTracker&) = delete;
  OtpMetricsTracker& operator=(const OtpMetricsTracker&) = delete;
  virtual ~OtpMetricsTracker();

  // Called when an OTP field is detected in a form. `autofill_manager` owns the
  // OTP form and is used to lazily evaluate user interaction with `field_ids`
  // in `form_id` upon tickle arrival.
  void OnOtpFieldDetected(FormGlobalId form_id,
                          std::vector<FieldGlobalId> field_ids,
                          AutofillManager& autofill_manager);

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
  void RecordFormOutcomeMetrics(
      one_time_tokens::TickleFormOutcome form_outcome);

  // Returns the language of the page associated with the detected OTP form.
  // Queries `autofill_client_` for the latest detected language if available,
  // falling back to `page_language_` (cached during `OnOtpFieldDetected()`) if
  // its current language is empty.
  std::string GetOtpPageLanguage() const;
  bool IsOtpFieldEmptyAndUnedited() const;
  void ResetPendingFormState();

  raw_ptr<one_time_tokens::OneTimeTokenService> one_time_token_service_;
  // The owning `AutofillClient`, guaranteed to outlive this class.
  const raw_ref<AutofillClient> autofill_client_;
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

  // Main frame UKM source ID of the page where the most recent OTP field was
  // detected. Caching this is necessary because a tickle may arrive after the
  // user has navigated or submitted the form, at which point
  // `autofill_manager_` is null.
  std::optional<ukm::SourceId> ukm_source_id_;

  // Language of the page where the most recent OTP field was detected. Caching
  // this is necessary because a tickle may arrive or timeout may fire after
  // the user has navigated or submitted the form, at which point
  // `autofill_manager_` is null.
  std::optional<std::string> page_language_;

  // Speculative page language recorded when a tickle arrived without any
  // previously detected OTP field. Used for
  // `kPageLanguageNoFieldDetectedHistogram` if the tickle expires without a
  // field ever being detected.
  std::optional<std::string> speculative_page_language_;

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
