// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SOURCE_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {

// Interface for modular, tab-scoped OTP sources (`SmsOtpSource`,
// `GmailOtpSource`) owned by `OtpCoordinator` for the lifetime of the tab.
// Sources can be started and stopped across multiple `Start()`/`Stop()` cycles
// (e.g. across navigations or form lifecycles), and `Start()` may be called
// repeatedly while active to renew subscriptions or add new origins.
class OtpSource {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    // Invoked when a new candidate (or retrieval error) arrives from the
    // source. If `target` is `std::nullopt`, the candidate applies to the
    // active target resolved by `OtpCoordinator` (or any target matching
    // `candidate.verified_origin`).
    virtual void OnSourceCandidateAvailable(
        std::optional<OtpTarget> target,
        base::expected<OtpCandidate,
                       one_time_tokens::OneTimeTokenRetrievalError>
            candidate_or_error) = 0;
  };

  virtual ~OtpSource() = default;

  // Starts or renews listening for `target`. Idempotent if already listening
  // for `target`'s origin/channel.
  virtual void Start(const OtpTarget& target) = 0;

  // Stops active subscriptions when OTP fields disappear or on navigation.
  // Implementations may retain cached unconsumed OTPs until their cache expiry
  // unless explicitly cleared.
  virtual void Stop() = 0;

  // Stops active subscriptions and clears cached unconsumed OTPs scoped to
  // `origin` when all OTP fields for that origin disappear, while leaving
  // subscriptions and caches for other origins active.
  virtual void StopListeningForOrigin(const url::Origin& origin) {}

  // Returns any currently available, unconsumed OTP candidates for `target`.
  virtual std::vector<OtpCandidate> GetAvailable(
      const OtpTarget& target) const = 0;

  // Marks `identity` as consumed so it is not offered again.
  virtual void MarkConsumed(const OtpTokenIdentity& identity) = 0;

  // Clears any cached OTPs for `origin` (e.g. upon a PhishGuard phishing
  // verdict).
  virtual void ClearVerifiedCacheForOrigin(const url::Origin& origin) = 0;

  // Clears all cached verified OTPs across all origins (e.g. on form
  // submission/removal or navigation).
  virtual void ClearAllVerifiedCache() = 0;

  // Sets or clears the observer notified when candidates arrive. `observer` may
  // be null to unregister; if non-null, the source retains an unowned pointer
  // and `*observer` must outlive this `OtpSource` or unregister via
  // `SetObserver(nullptr)` before destruction.
  virtual void SetObserver(Observer* observer) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SOURCE_H_
