// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/phosphor/feature_token_manager.h"

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
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/data_types.h"
#include "components/legion/phosphor/token_fetcher.h"
#include "net/base/features.h"

namespace legion::phosphor::internal {

namespace {

auto kBlindSignedAuthTokenComparator = [](const BlindSignedAuthToken& a,
                                          const BlindSignedAuthToken& b) {
  return a.expiration > b.expiration;
};

}  // namespace

FeatureTokenManager::FeatureTokenManager(TokenFetcher* fetcher,
                                         int batch_size,
                                         size_t cache_low_water_mark)
    : batch_size_(batch_size),
      cache_low_water_mark_(cache_low_water_mark),
      fetcher_(fetcher) {}

FeatureTokenManager::~FeatureTokenManager() = default;

bool FeatureTokenManager::IsAuthTokenAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();
  return !cache_.empty();
}

std::optional<BlindSignedAuthToken> FeatureTokenManager::GetAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();

  std::optional<BlindSignedAuthToken> result;
  if (!cache_.empty()) {
    std::pop_heap(cache_.begin(), cache_.end(),
                  kBlindSignedAuthTokenComparator);
    result.emplace(std::move(cache_.back()));
    cache_.pop_back();
  }

  VLOG(2) << "Legion ATC::GetAuthToken with " << cache_.size()
          << " tokens available";
  MaybeRefillCache();
  return result;
}

void FeatureTokenManager::OnGotAuthTokens(
    base::expected<std::vector<BlindSignedAuthToken>, base::Time> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetching_auth_tokens_ = false;

  if (!result.has_value()) {
    VLOG(2) << "Legion ATC::OnGotAuthTokens back off until " << result.error();
    try_get_auth_tokens_after_ = result.error();
    ScheduleMaybeRefillCache();
    return;
  }

  VLOG(2) << "Legion ATC::OnGotAuthTokens got " << result->size() << " tokens";
  try_get_auth_tokens_after_.reset();

  RemoveExpiredTokens();

  if (result->empty()) {
    VLOG(1) << "Legion ATC::OnGotAuthTokens got an empty list of tokens. "
               "Treating as a transient error.";
    // TODO(b:457425177): Record a UMA metric for this case.
    try_get_auth_tokens_after_ =
        base::Time::Now() +
        legion::kLegionTryGetAuthTokensTransientBackoff.Get();
    ScheduleMaybeRefillCache();
    return;
  }

  for (auto& token : *result) {
    cache_.push_back(std::move(token));
    std::push_heap(cache_.begin(), cache_.end(),
                   kBlindSignedAuthTokenComparator);
  }

  ScheduleMaybeRefillCache();
}

void FeatureTokenManager::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now();
  while (!cache_.empty() && cache_.front().expiration <= fresh_after) {
    std::pop_heap(cache_.begin(), cache_.end(),
                  kBlindSignedAuthTokenComparator);
    cache_.pop_back();
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
    VLOG(2) << "Legion ATC::MaybeRefillCache calling GetAuthnTokens";
    // base::Unretained is unsafe here, because the FeatureTokenManager can be
    // destroyed while a token fetch is in progress.
    fetcher_->GetAuthnTokens(
        batch_size_, base::BindOnce(&FeatureTokenManager::OnGotAuthTokens,
                                    weak_ptr_factory_.GetWeakPtr()));
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
  return cache_.size() < cache_low_water_mark_;
}

}  // namespace legion::phosphor::internal
