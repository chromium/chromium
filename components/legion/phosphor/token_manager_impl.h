// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_IMPL_H_
#define COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/legion/phosphor/token_manager.h"

namespace legion::phosphor {

class TokenFetcher;

namespace internal {
class FeatureTokenManager;
}  // namespace internal

// An implementation of TokenManager that populates itself
// using a passed in TokenFetcher pointer from the cache.
class TokenManagerImpl : public TokenManager {
 public:
  explicit TokenManagerImpl(std::unique_ptr<TokenFetcher> fetcher);
  ~TokenManagerImpl() override;

  // TokenManager implementation.
  void GetAuthToken(GetAuthTokenCallback callback) override;
  void PrefetchAuthTokens() override;

 private:
  const int batch_size_;
  const size_t cache_low_water_mark_;

  std::unique_ptr<TokenFetcher> fetcher_;

  std::unique_ptr<internal::FeatureTokenManager> feature_token_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TokenManagerImpl> weak_ptr_factory_{this};
};

}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_TOKEN_MANAGER_IMPL_H_
