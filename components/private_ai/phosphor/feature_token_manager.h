// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_FEATURE_TOKEN_MANAGER_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_FEATURE_TOKEN_MANAGER_H_

#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/phosphor/token_manager.h"

namespace base {
class OneShotTimer;
}

namespace quiche {
enum class ProxyLayer;
}  // namespace quiche

namespace private_ai::phosphor {

class TokenFetcher;

namespace internal {

// Manages tokens for a single feature.
// This class should not be used outside of the Phosphor component.
class FeatureTokenManager {
 public:
  FeatureTokenManager(TokenFetcher* fetcher,
                      quiche::ProxyLayer proxy_layer,
                      int batch_size,
                      size_t cache_low_water_mark);
  ~FeatureTokenManager();

  FeatureTokenManager(const FeatureTokenManager&) = delete;
  FeatureTokenManager& operator=(const FeatureTokenManager&) = delete;

  // Gets a token for the feature asynchronously.
  void GetAuthToken(TokenManager::GetAuthTokenCallback callback);

  // Ensures that tokens are available for the feature, fetching them if
  // necessary. This method is intended for pre-fetching and does not return a
  // token.
  void PrefetchAuthTokens();

 private:
  // The callback for token fetch completion. On success, this receives a
  // vector of tokens. On failure, it receives the time after which a retry is
  // permitted.
  void OnGotAuthTokens(
      base::expected<std::vector<BlindSignedAuthToken>, base::Time> result);

  // Runs all pending callbacks with a `std::nullopt` token.
  void FailPendingCallbacks();

  // Removes expired tokens from the cache.
  void RemoveExpiredTokens();

  // If the cache needs to be refilled, this will trigger a fetch of tokens
  // from the `TokenFetcher`.
  void MaybeRefillCache();

  // Schedules `MaybeRefillCache` to run at the appropriate time.
  void ScheduleMaybeRefillCache();

  // Returns true if the cache should be refilled with new tokens.
  bool NeedsRefill() const;

  const quiche::ProxyLayer proxy_layer_;
  const int batch_size_;
  const size_t cache_low_water_mark_;

  // Cache of tokens, maintained as a queue sorted by expiration time.
  std::deque<BlindSignedAuthToken> cache_;

  // Callbacks for `GetAuthToken` that are waiting for a fetch to complete.
  std::deque<TokenManager::GetAuthTokenCallback> pending_callbacks_;

  raw_ptr<TokenFetcher> fetcher_;

  // The set of features for which a token fetch is currently in progress.
  bool fetching_auth_tokens_ = false;

  // If not null, this is the `try_again_after` time from the last call to
  // `GetAuthnTokens()`, and no calls should be made until this time.
  std::optional<base::Time> try_get_auth_tokens_after_;

  std::unique_ptr<base::OneShotTimer> refill_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FeatureTokenManager> weak_ptr_factory_{this};
};

}  // namespace internal
}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_FEATURE_TOKEN_MANAGER_H_
