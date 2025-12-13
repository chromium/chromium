// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_FEATURE_TOKEN_MANAGER_H_
#define COMPONENTS_LEGION_PHOSPHOR_FEATURE_TOKEN_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/legion/phosphor/data_types.h"

namespace base {
class OneShotTimer;
}

namespace legion::phosphor {

class TokenFetcher;

namespace internal {

// Manages tokens for a single feature.
// This class should not be used outside of the Phosphor component.
class FeatureTokenManager {
 public:
  FeatureTokenManager(TokenFetcher* fetcher,
                      int batch_size,
                      size_t cache_low_water_mark);
  ~FeatureTokenManager();

  FeatureTokenManager(const FeatureTokenManager&) = delete;
  FeatureTokenManager& operator=(const FeatureTokenManager&) = delete;

  // Checks if there are any non-expired tokens in the cache.
  bool IsAuthTokenAvailable();

  // Returns a token from the cache if one is available. This will trigger a
  // refill of the cache if it is below the low water mark.
  std::optional<BlindSignedAuthToken> GetAuthToken();

 private:
  void OnGotAuthTokens(base::TimeTicks,
                       std::optional<std::vector<BlindSignedAuthToken>> tokens,
                       std::optional<base::Time> try_again_after);

  // Removes expired tokens from the cache.
  void RemoveExpiredTokens();

  // If the cache needs to be refilled, this will trigger a fetch of tokens
  // from the `TokenFetcher`.
  void MaybeRefillCache();

  // Schedules `MaybeRefillCache` to run at the appropriate time.
  void ScheduleMaybeRefillCache();

  // Returns true if the cache should be refilled with new tokens.
  bool NeedsRefill() const;

  const int batch_size_;
  const size_t cache_low_water_mark_;

  // Cache of tokens, maintained as a min-heap sorted by expiration time.
  std::vector<BlindSignedAuthToken> cache_;

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
}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_FEATURE_TOKEN_MANAGER_H_
