// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/phosphor/token_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/feature_token_manager.h"
#include "components/legion/phosphor/token_fetcher.h"

namespace legion::phosphor {

TokenManagerImpl::TokenManagerImpl(std::unique_ptr<TokenFetcher> fetcher)
    : batch_size_(legion::kLegionAuthTokenCacheBatchSize.Get()),
      cache_low_water_mark_(legion::kLegionAuthTokenCacheLowWaterMark.Get()),
      fetcher_(std::move(fetcher)) {}

TokenManagerImpl::~TokenManagerImpl() = default;

internal::FeatureTokenManager* TokenManagerImpl::GetOrCreateFeatureManager(
    proto::FeatureName feature_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = feature_managers_.find(feature_name);
  if (it != feature_managers_.end()) {
    return it->second.get();
  }

  auto new_manager = std::make_unique<internal::FeatureTokenManager>(
      fetcher_.get(), batch_size_, cache_low_water_mark_);
  auto* new_manager_ptr = new_manager.get();
  feature_managers_[feature_name] = std::move(new_manager);
  return new_manager_ptr;
}

bool TokenManagerImpl::IsAuthTokenAvailable(proto::FeatureName feature_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = feature_managers_.find(feature_name);
  if (it == feature_managers_.end()) {
    return false;
  }
  return it->second->IsAuthTokenAvailable();
}

std::optional<BlindSignedAuthToken> TokenManagerImpl::GetAuthToken(
    proto::FeatureName feature_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetOrCreateFeatureManager(feature_name)->GetAuthToken();
}

}  // namespace legion::phosphor
