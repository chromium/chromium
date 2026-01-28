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
      fetcher_(std::move(fetcher)) {
  feature_token_manager_ = std::make_unique<internal::FeatureTokenManager>(
      fetcher_.get(), batch_size_, cache_low_water_mark_);
}

TokenManagerImpl::~TokenManagerImpl() = default;

void TokenManagerImpl::GetAuthToken(GetAuthTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  feature_token_manager_->GetAuthToken(std::move(callback));
}

void TokenManagerImpl::PrefetchAuthTokens() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  feature_token_manager_->PrefetchAuthTokens();
}

}  // namespace legion::phosphor
