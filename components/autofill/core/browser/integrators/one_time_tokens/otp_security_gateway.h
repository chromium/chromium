// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SECURITY_GATEWAY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SECURITY_GATEWAY_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery_policy.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_gateway.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"

namespace autofill {

class AutofillClient;

// Tab-scoped security gateway owned by `OtpCoordinator` that validates OTP
// candidates via `OtpPhishGuardGateway` and `OtpDeliveryPolicy`. Validation
// results are tied to the `OtpTarget` and `OtpCandidate` they were requested
// for.
class OtpSecurityGateway {
 public:
  using ValidationCallback =
      base::OnceCallback<void(OtpTarget target,
                              OtpCandidate candidate,
                              OtpDeliveryPolicy::Decision decision)>;

  struct ProactiveTriggerContext {
    // Defaults to fail-closed values that suppress proactive triggering if a
    // caller omits a field, except `is_keyboard_visible` until Android
    // soft-keyboard visibility is plumbed (crbug.com/451991285).
    bool target_field_exists = false;
    bool frame_active = false;
    bool is_otp_field_focused = false;
    // TODO(crbug.com/451991285): Plumb real Android soft-keyboard visibility
    // signal into `is_keyboard_visible` and flip this default to `false`.
    bool is_keyboard_visible = true;
    bool has_non_otp_focus_anywhere = true;
    bool any_otp_field_has_typed_input = true;
  };

  explicit OtpSecurityGateway(AutofillClient& client);
  OtpSecurityGateway(const OtpSecurityGateway&) = delete;
  OtpSecurityGateway& operator=(const OtpSecurityGateway&) = delete;
  ~OtpSecurityGateway();

  // Runs PhishGuard and policy verification for `candidate` targeting `target`.
  // When allowed, stamps `candidate.verified_frame` (and `verified_origin` for
  // unbound SMS candidates) with `target` before invoking `callback`.
  void ValidateDelivery(OtpTarget target,
                        OtpCandidate candidate,
                        ValidationCallback callback);

  // Evaluates synchronous policy checks for proactive triggering of a
  // `candidate` that has already passed `ValidateDelivery` for `target`. Fails
  // closed if `candidate.verified_frame != target.frame` or if live consent,
  // origin, WebOTP, or UX preconditions are not met.
  OtpDeliveryPolicy::Decision EvaluateProactiveTrigger(
      const OtpTarget& target,
      const OtpCandidate& candidate,
      const ProactiveTriggerContext& context) const;

  // Evaluates synchronous eligibility for serving a `candidate` on a reactive
  // UI request after it has already passed `ValidateDelivery` for `target`.
  // Fails closed if `candidate.verified_frame != target.frame` or if live
  // consent, origin, or WebOTP preconditions are not met.
  OtpDeliveryPolicy::Decision EvaluateReactiveServing(
      const OtpTarget& target,
      const OtpCandidate& candidate) const;

 private:
  void OnPhishGuardVerdict(OtpTarget target,
                           OtpCandidate candidate,
                           ValidationCallback callback,
                           OneTimeTokensPhishGuardVerdict verdict);

  OtpDeliveryPolicy::Decision EvaluateVerificationPolicy(
      const OtpTarget& target,
      const OtpCandidate& candidate,
      OneTimeTokensPhishGuardVerdict verdict) const;

  bool UserOptedIntoGmailOtpFilling() const;

  const raw_ref<AutofillClient> client_;
  OtpPhishGuardGateway phish_guard_gateway_;

  base::WeakPtrFactory<OtpSecurityGateway> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SECURITY_GATEWAY_H_
