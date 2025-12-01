// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"

namespace one_time_tokens {

// The origin of a one time token. This is currently limited to on-device SMS
// tokens, but may grow to cross-device SMS tokens, email tokens, authenticator
// tokens, etc. in the future. As SMS tokens can come from different sources
// this is not identical to `OneTimeTokenType`.
enum class OneTimeTokenSource {
  kUnknown = 0,
  kOnDeviceSms = 1,
};

// Service to subscribe to `OneTimeToken`s. One instance per profile.
class OneTimeTokenService {
 public:
  using CallbackSignature =
      void(OneTimeTokenSource,
           base::expected<OneTimeToken, OneTimeTokenRetrievalError>);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  virtual ~OneTimeTokenService() = default;

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

  // Creates a subscription for new incoming one time tokens. It's possible that
  // the same one time token is reported many times while a subscription is
  // active. It's the responsibility of the caller to deduplicate those.
  [[nodiscard]] virtual ExpiringSubscription Subscribe(base::Time expiration,
                                                       Callback callback) = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_H_
