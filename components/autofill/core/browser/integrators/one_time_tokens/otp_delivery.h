// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_DELIVERY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_DELIVERY_H_

#include <optional>

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"

namespace autofill {

// Explicit data state per target OTP field, owned by `OtpCoordinator`.
// Holds the newest verified OTP candidate and enforces the monotonic
// newest-wins guarantee across async hops. Screen states (pending, shown,
// dismissed) are owned by `OtpSuggestionDropdownDispatcher`.
class OtpDelivery {
 public:
  explicit OtpDelivery(OtpTarget target);
  OtpDelivery(const OtpDelivery&);
  OtpDelivery& operator=(const OtpDelivery&);
  OtpDelivery(OtpDelivery&&) noexcept;
  OtpDelivery& operator=(OtpDelivery&&) noexcept;
  ~OtpDelivery();

  const OtpTarget& target() const LIFETIME_BOUND { return target_; }
  DeliveryState GetState() const;
  std::optional<OtpTokenIdentity> GetNewestIdentity() const;

  // Returns true if `candidate` can be accepted by this delivery state (not
  // consumed, not expired, matching origin, field still active, not blocked by
  // phishing, and strictly newer than any existing `newest_verified_`).
  bool CanAccept(const OtpCandidate& candidate) const;

  // Offers a verified `candidate`. Accepts it only if `CanAccept()` returns
  // true.
  bool Offer(OtpCandidate candidate);

  // Returns the current unconsumed, unexpired verified candidate if available.
  std::optional<OtpCandidate> GetAvailableCandidate() const;

  // Called when the user fills the OTP or submits the form. Prevents the
  // filled token from being served again while retaining its identity as the
  // monotonic floor so older tokens cannot overwrite it if a resend arrives.
  // Returns the identity of the consumed candidate if one was present.
  std::optional<OtpTokenIdentity> MarkConsumed();

  // Called when the target field is removed, navigated away from, or submitted
  // so that late-arriving async candidates for this stale field are dropped.
  void MarkFieldGone();

  // Called when PhishGuard reports a phishing verdict for this target. Clears
  // any stored candidate and permanently blocks future candidates for this
  // target. After a phishing verdict, the delivery accepts nothing and reports
  // `DeliveryState::kListening` (rather than `DeliveryState::kFieldGone`, which
  // would cause `OtpCoordinator` to replace the delivery and lose the block).
  void BlockOnPhishing();

 private:
  bool HasAvailableCandidate() const;

  OtpTarget target_;
  bool field_gone_ = false;
  bool blocked_by_phishing_ = false;
  std::optional<OtpCandidate> newest_verified_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_DELIVERY_H_
