// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_SERVICE_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace one_time_tokens {

// Duration after which tokens expire and are no longer returned or served.
inline constexpr base::TimeDelta kCacheDurationForOldTokens = base::Minutes(3);

class OneTimeTokenLogSink;

// Service to subscribe to `OneTimeToken`s. One instance per profile.
class SmsOtpService : public KeyedService {
 public:
  using CallbackSignature =
      void(base::expected<OneTimeToken, OneTimeTokenRetrievalError>);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  ~SmsOtpService() override = default;

  // Calls `callback` with tokens that were received in the recent past (if any
  // exist). `callback` may be called multiple times for this. This should
  // always be followed by a subscription because only cached tokens are
  // returned. It's possible that a backend will find tokens from the past that
  // are not in the cache, yet. This function should not return any errors that
  // may not be valid anymore (like, e.g., errors about incomplete permissions
  // or expired subscriptions). No callback happens if no (unexpired) tokens are
  // cached.
  virtual void GetRecentOneTimeTokens(Callback callback) = 0;

  // Returns the cached one-time tokens.
  virtual std::vector<OneTimeToken> GetCachedOneTimeTokens() const = 0;

  // Creates a subscription for new incoming on-device SMS one time tokens.
  // It's possible that the same one time token is reported many times while a
  // subscription is active. It's the responsibility of the caller to
  // deduplicate those.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(
      base::Time expiration,
      Callback callback,
      base::OnceClosure expiration_callback) = 0;

  // Requests one time tokens from the underlying backend. `callback` is called
  // exactly once when the request is complete, with the fetched token if
  // successful. The `callback` is called with `std::nullopt` if no token could
  // be fetched within the `timeout`, if the backend is not available or if
  // there is a backend error.
  virtual void RequestOneTimeToken(
      base::TimeDelta timeout,
      base::OnceCallback<void(std::optional<OneTimeToken>)> callback) = 0;

  virtual OneTimeTokenLogSink* log_sink() = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_SERVICE_H_
