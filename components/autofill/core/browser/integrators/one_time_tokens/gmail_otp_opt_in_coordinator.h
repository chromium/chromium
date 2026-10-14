// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_COORDINATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_COORDINATOR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace base {
class SupportsUserData;
}  // namespace base

namespace one_time_tokens {
class GmailOtpBackend;
}

namespace autofill {

class AutofillClient;

// Tab-scoped coordinator that correlates OTP field detections with incoming
// Gmail push notifications (tickles) across frame navigations, evaluates gating
// conditions, and triggers the Gmail OTP Opt-In Bubble when appropriate.
//
// Owned by `ChromeAutofillClient` (one instance per `WebContents`).
class GmailOtpOptInCoordinator {
 public:
  // Duration for which the coordinator subscribes to Gmail tickles after an OTP
  // field is detected.
  static constexpr base::TimeDelta kTickleSubscriptionDuration =
      base::Minutes(3);

  // Cooldown duration after the user clicks "No thanks" (`kDeclined`) before
  // the opt-in bubble can be shown again.
  static constexpr base::TimeDelta kDeclinedCooldownDuration = base::Days(90);

  // If `gmail_otp_backend` is null, no tickles are received.
  // `session_user_data` is a profile/session-scoped object used to record
  // in-memory dismissal state across tabs for the duration of the browser
  // session.
  GmailOtpOptInCoordinator(AutofillClient& client,
                           one_time_tokens::GmailOtpBackend* gmail_otp_backend,
                           base::SupportsUserData& session_user_data);
  GmailOtpOptInCoordinator(const GmailOtpOptInCoordinator&) = delete;
  GmailOtpOptInCoordinator& operator=(const GmailOtpOptInCoordinator&) = delete;
  ~GmailOtpOptInCoordinator();

 private:
  void OnOtpFieldsDetected();
  void OnTickleReceived();
  void OnOptInBubbleClosed(GmailOtpOptInResult result);

  const raw_ref<AutofillClient> client_;
  const raw_ptr<one_time_tokens::GmailOtpBackend> gmail_otp_backend_;
  const raw_ref<base::SupportsUserData> session_user_data_;

  base::CallbackListSubscription otp_fields_detected_subscription_;
  one_time_tokens::ExpiringSubscription tickle_subscription_;

  // Tracks whether an opt-in bubble request is currently in flight. Prevents
  // duplicate `ShowGmailOtpOptInBubble()` calls when multiple tickles arrive in
  // rapid succession, which would otherwise cause the UI controller to reject
  // the second request with `GmailOtpOptInResult::kDiscarded` and invoke
  // `OnOptInBubbleClosed()` while the first bubble is still open. Using a
  // boolean flag is also safer than resetting `tickle_subscription_`, because
  // `OnTickleReceived()` can be invoked synchronously inside
  // `SubscribeToTickles()` before `tickle_subscription_` is assigned.
  bool is_bubble_showing_ = false;

  base::WeakPtrFactory<GmailOtpOptInCoordinator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_COORDINATOR_H_
