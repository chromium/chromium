// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_SOURCE_H_

#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/origin.h"

namespace one_time_tokens {
class GmailOtpService;
}  // namespace one_time_tokens

namespace autofill {

class AutofillClient;

// Tab-scoped Gmail OTP source owned by `OtpCoordinator` (one instance per tab,
// supporting repeated `Start()`, `StopListeningForOrigin()`, and `Stop()`
// cycles across DOM updates and navigations).
// Maintains one continuous `GmailOtpService` subscription per active non-opaque
// origin (for opted-in users only, renewing in place on repeated `Start()`
// calls and across expiration callbacks while the origin remains active),
// queries the service's freshest cached matching OTP on initial subscription,
// and keeps a per-origin store where the strictly newest unexpired `kGmail` OTP
// wins and consumed OTPs are excluded. Asynchronous notifications emit
// `std::nullopt` target and an origin-verified `OtpCandidate` so
// `OtpCoordinator` resolves the live target field and frame at delivery time.
class GmailOtpSource : public OtpSource {
 public:
  // `client` must outlive `this`. `client.GetGmailOtpService()` is queried once
  // at construction; if null (e.g. in Incognito or on unsupported platforms),
  // `Start()` is a no-op, otherwise the returned service must outlive `this`.
  explicit GmailOtpSource(AutofillClient& client);
  GmailOtpSource(const GmailOtpSource&) = delete;
  GmailOtpSource& operator=(const GmailOtpSource&) = delete;
  ~GmailOtpSource() override;

  // OtpSource:
  void Start(const OtpTarget& target) override;
  void Stop() override;
  void StopListeningForOrigin(const url::Origin& origin) override;
  std::vector<OtpCandidate> GetAvailable(
      const OtpTarget& target) const override;
  void MarkConsumed(const OtpTokenIdentity& identity) override;
  void ClearVerifiedCacheForOrigin(const url::Origin& origin) override;
  void ClearAllVerifiedCache() override;
  // `observer` may be null to unregister; if non-null, `GmailOtpSource` retains
  // an unowned pointer and `*observer` must outlive `this` or unregister via
  // `SetObserver(nullptr)` before destruction.
  void SetObserver(Observer* observer) override;

  bool HasActiveSubscriptionForOriginForTesting(
      const url::Origin& origin) const;

 private:
  struct OrderByOnDeviceArrival {
    using is_transparent = void;

    bool operator()(const OtpTokenIdentity& lhs,
                    const OtpTokenIdentity& rhs) const;
    bool operator()(const OtpTokenIdentity& lhs,
                    base::TimeTicks rhs_arrival) const;
    bool operator()(base::TimeTicks lhs_arrival,
                    const OtpTokenIdentity& rhs) const;
  };

  void EnsureContinuousSubscriptionForOrigin(const url::Origin& origin);
  void FetchFreshestCachedOtpForOrigin(const url::Origin& origin);
  one_time_tokens::ExpiringSubscription SubscribeToService(
      const url::Origin& origin,
      base::Time expiration);
  void OnGmailOtpFillingEnabledPrefChanged();
  void OnCachedOtpRetrieved(const url::Origin& origin,
                            std::optional<one_time_tokens::OneTimeToken> token);
  void OnIncomingTokenReceived(
      const url::Origin& origin,
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError> result);
  bool ProcessMatchedToken(const url::Origin& origin,
                           const one_time_tokens::OneTimeToken& token);

  bool HasActiveSubscriptionForOrigin(const url::Origin& origin) const;
  bool UserOptedIntoGmailOtpFilling() const;
  void PruneExpiredConsumedTokens(base::TimeTicks now);

  const raw_ref<AutofillClient> client_;
  const raw_ptr<one_time_tokens::GmailOtpService> gmail_otp_service_;
  raw_ptr<Observer> observer_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;

  // Keyed by every non-opaque origin currently holding active OTP fields in
  // this tab. Holds a live subscription while the user is opted into Gmail OTP
  // filling, or an inactive entry otherwise so subscriptions can start if the
  // user opts in mid-page.
  base::flat_map<url::Origin, one_time_tokens::ExpiringSubscription>
      continuous_subscriptions_per_origin_;

  // Domain-matched Gmail OTP store keyed by origin where strictly newer wins.
  base::flat_map<url::Origin, one_time_tokens::OneTimeToken>
      verified_store_per_origin_;

  // Consumed Gmail OTP identities ordered by `on_device_arrival` so expired
  // entries can be pruned from the front in amortized O(log N) time.
  std::set<OtpTokenIdentity, OrderByOnDeviceArrival> consumed_tokens_;

  base::WeakPtrFactory<GmailOtpSource> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_GMAIL_OTP_SOURCE_H_
