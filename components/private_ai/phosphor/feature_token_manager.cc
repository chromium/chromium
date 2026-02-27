// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/feature_token_manager.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/phosphor/token_fetcher.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "net/base/features.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"

namespace private_ai::phosphor::internal {

FeatureTokenManager::FeatureTokenManager(TokenFetcher* fetcher,
                                         quiche::ProxyLayer proxy_layer,
                                         int batch_size,
                                         size_t cache_low_water_mark)
    : proxy_layer_(proxy_layer),
      batch_size_(batch_size),
      cache_low_water_mark_(cache_low_water_mark),
      fetcher_(fetcher) {}

FeatureTokenManager::~FeatureTokenManager() = default;

void FeatureTokenManager::PrefetchAuthTokens() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeRefillCache();
}

void FeatureTokenManager::GetAuthToken(
    TokenManager::GetAuthTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();

  base::UmaHistogramBoolean(
      "PrivateAi.Phosphor.FeatureTokenManager.ServedFromCache",
      !cache_.empty());
  if (!cache_.empty()) {
    std::optional<BlindSignedAuthToken> result;
    result.emplace(std::move(cache_.front()));
    cache_.pop_front();

    VLOG(2) << "PrivateAI ATC::GetAuthToken with " << cache_.size()
            << " tokens available";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  } else {
    VLOG(2) << "PrivateAI ATC::GetAuthToken with no tokens available, queuing "
               "request";
    pending_callbacks_.push_back(std::move(callback));
  }
  MaybeRefillCache();
}

void FeatureTokenManager::FailPendingCallbacks() {
  for (auto& callback : std::exchange(pending_callbacks_, {})) {
    std::move(callback).Run(std::nullopt);
  }
}

void FeatureTokenManager::OnGotAuthTokens(
    base::expected<std::vector<BlindSignedAuthToken>, base::Time> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetching_auth_tokens_ = false;

  if (!result.has_value()) {
    VLOG(2) << "PrivateAI ATC::OnGotAuthTokens back off until "
            << result.error();
    base::UmaHistogramCounts100(
        "PrivateAi.Phosphor.FeatureTokenManager.TokensFetched", 0);
    try_get_auth_tokens_after_ = result.error();
    FailPendingCallbacks();
    ScheduleMaybeRefillCache();
    return;
  }

  VLOG(2) << "PrivateAI ATC::OnGotAuthTokens got " << result->size()
          << " tokens";
  base::UmaHistogramCounts100(
      "PrivateAi.Phosphor.FeatureTokenManager.TokensFetched", result->size());
  try_get_auth_tokens_after_.reset();

  RemoveExpiredTokens();

  if (result->empty()) {
    VLOG(1) << "PrivateAI ATC::OnGotAuthTokens got an empty list of tokens. "
               "Treating as a transient error.";
    try_get_auth_tokens_after_ =
        base::Time::Now() + kPrivateAiTryGetAuthTokensTransientBackoff.Get();
    FailPendingCallbacks();
    ScheduleMaybeRefillCache();
    return;
  }

  for (auto& token : *result) {
    cache_.push_back(std::move(token));
  }

  while (!cache_.empty() && !pending_callbacks_.empty()) {
    std::optional<BlindSignedAuthToken> token;
    token.emplace(std::move(cache_.front()));
    cache_.pop_front();
    TokenManager::GetAuthTokenCallback callback =
        std::move(pending_callbacks_.front());
    pending_callbacks_.pop_front();
    std::move(callback).Run(std::move(token));
  }

  ScheduleMaybeRefillCache();
}

void FeatureTokenManager::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now();
  while (!cache_.empty() && cache_.front().expiration <= fresh_after) {
    cache_.pop_front();
  }
}

void FeatureTokenManager::MaybeRefillCache() {
  RemoveExpiredTokens();
  if (fetching_auth_tokens_ || !fetcher_) {
    return;
  }

  if (try_get_auth_tokens_after_.has_value() &&
      base::Time::Now() < *try_get_auth_tokens_after_) {
    ScheduleMaybeRefillCache();
    return;
  }

  if (NeedsRefill()) {
    fetching_auth_tokens_ = true;
    VLOG(2) << "PrivateAI ATC::MaybeRefillCache calling GetAuthnTokens";
    // base::Unretained is unsafe here, because the FeatureTokenManager can be
    // destroyed while a token fetch is in progress.
    fetcher_->GetAuthnTokens(
        batch_size_, proxy_layer_,
        base::BindOnce(&FeatureTokenManager::OnGotAuthTokens,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  ScheduleMaybeRefillCache();
}

void FeatureTokenManager::ScheduleMaybeRefillCache() {
  if (fetching_auth_tokens_ || !fetcher_) {
    refill_timer_ = nullptr;
    return;
  }

  base::Time now = base::Time::Now();
  base::Time next_try = now;

  if (NeedsRefill()) {
    next_try = try_get_auth_tokens_after_.value_or(now);
  } else if (!cache_.empty()) {
    next_try = cache_.front().expiration;
  }

  const base::TimeDelta delay = std::max(base::TimeDelta(), next_try - now);

  if (!refill_timer_) {
    refill_timer_ = std::make_unique<base::OneShotTimer>();
  }
  refill_timer_->Start(FROM_HERE, delay,
                       base::BindOnce(&FeatureTokenManager::MaybeRefillCache,
                                      weak_ptr_factory_.GetWeakPtr()));
}

bool FeatureTokenManager::NeedsRefill() const {
  return !pending_callbacks_.empty() || cache_.size() < cache_low_water_mark_;
}

}  // namespace private_ai::phosphor::internal
