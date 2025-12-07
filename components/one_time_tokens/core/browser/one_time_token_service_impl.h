// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_IMPL_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_cache.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

namespace one_time_tokens {

// The GMSCore API returns SMS that were received up to one minute ago. This
// means that the first call to fetch SMS may return a stale SMS OTPs and we
// need to keep polling to learn about more recent OTPs. This interval specifies
// how often a request is submitted even if the API returned a SMS.
inline constexpr base::TimeDelta kSmsRefetchInterval = base::Seconds(5);

// Duration after which tokens expire and won't be returned by
// `GetRecentOneTimeTokens`.
inline constexpr base::TimeDelta kCacheDurationForOldTokens = base::Minutes(3);

// Service to subscribe to one time tokens. One instance per profile.
class OneTimeTokenServiceImpl : public OneTimeTokenService,
                                public KeyedService {
 public:
  // If `sms_otp_backend` is null, this class does not do any subscriptions.
  explicit OneTimeTokenServiceImpl(SmsOtpBackend* sms_otp_backend);
  OneTimeTokenServiceImpl(const OneTimeTokenServiceImpl&) = delete;
  OneTimeTokenServiceImpl& operator=(const OneTimeTokenServiceImpl&) = delete;
  ~OneTimeTokenServiceImpl() override;

  // OneTimeTokenService:
  void GetRecentOneTimeTokens(Callback callback) override;
  [[nodiscard]] ExpiringSubscription Subscribe(base::Time expiration,
                                               Callback callback) override;
  std::vector<OneTimeToken> GetCachedOneTimeTokens() const override;

 private:
  // Retrieves SMS OTPs from `sms_.backend` if any subscriber is interested.
  // Results are posted to `OnResponseFromSmsOtpBackend`.
  void RetrieveSmsOtpIfNeeded();
  void OnResponseFromSmsOtpBackend(const OtpFetchReply& reply);

  // Handles subscriptions to the `OneTimeTokenService`.
  ExpiringSubscriptionManager<CallbackSignature> subscription_manager_;

  // Handles requests of the `OneTimeTokenService` to the `SmsOtpBackend`.
  struct {
    bool has_pending_request = false;
    raw_ptr<SmsOtpBackend> backend;
  } sms_;

  OneTimeTokenCache cache_;

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<OneTimeTokenServiceImpl> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_IMPL_H_
