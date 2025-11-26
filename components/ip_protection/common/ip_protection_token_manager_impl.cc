// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_manager_impl.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "net/base/features.h"

namespace ip_protection {

namespace {

// Minimum time before actual expiration that a token is considered
// "expired" and removed. The maximum time is given by the
// `IpPrivacyExpirationFuzz` feature param.
constexpr base::TimeDelta kMinimumFuzzInterval = base::Seconds(5);

// Interval between measurements of the token rates.
constexpr base::TimeDelta kTokenRateMeasurementInterval = base::Minutes(5);

// Time delay used for immediate refill to prevent overloading servers.
constexpr base::TimeDelta kImmediateTokenRefillDelay = base::Minutes(1);

// Time delay used for when a token limit has been exceeded.
constexpr base::TimeDelta kTokenLimitExceededDelay = base::Minutes(10);

}  // namespace

IpProtectionTokenManagerImpl::IpProtectionTokenManagerImpl(
    IpProtectionCore* core,
    scoped_refptr<IpProtectionCoreHostRemote> core_host_remote,
    std::unique_ptr<IpProtectionTokenFetcher> fetcher,
    ProxyLayer proxy_layer,
    std::vector<BlindSignedAuthToken> initial_tokens,
    bool disable_cache_management_for_testing)
    : batch_size_(net::features::kIpPrivacyAuthTokenCacheBatchSize.Get()),
      cache_low_water_mark_(
          net::features::kIpPrivacyAuthTokenCacheLowWaterMark.Get()),
      fetcher_(std::move(fetcher)),
      proxy_layer_(proxy_layer),
      ip_protection_core_(core),
      core_host_remote_(std::move(core_host_remote)),
      disable_cache_management_for_testing_(
          disable_cache_management_for_testing) {
  ProcessInitialTokens(std::move(initial_tokens));
  last_token_rate_measurement_ = base::TimeTicks::Now();
  // Start the timer. The timer is owned by `this` and thus cannot outlive it.
  measurement_timer_.Start(FROM_HERE, kTokenRateMeasurementInterval, this,
                           &IpProtectionTokenManagerImpl::MeasureTokenRates);

  if (!disable_cache_management_for_testing_) {
    // Schedule a call to `MaybeRefillCache()`. This will occur soon, since the
    // cache is empty.
    ScheduleMaybeRefillCache();
  }
}

IpProtectionTokenManagerImpl::~IpProtectionTokenManagerImpl() {
  // Record orphaned (unspent, unexpired) tokens.
  RemoveExpiredTokens();
  std::vector<BlindSignedAuthToken> tokens;
  for (auto& [geo_id, cache] : cache_by_geo_) {
    Telemetry().RecordTokenCountEvent(
        proxy_layer_, IpProtectionTokenCountEvent::kOrphaned, cache.size());
    std::ranges::move(cache, std::back_inserter(tokens));
  }
  if (!tokens.empty()) {
    CHECK(core_host_remote_);
    core_host_remote_->core_host()->RecycleTokens(proxy_layer_,
                                                  std::move(tokens));
  }
}

void IpProtectionTokenManagerImpl::ProcessInitialTokens(
    std::vector<BlindSignedAuthToken> initial_tokens) {
  if (initial_tokens.empty()) {
    return;
  }

  for (auto& token : initial_tokens) {
    std::string geo_id = GetGeoIdFromGeoHint(token.geo_hint);
    cache_by_geo_[geo_id].push_back(std::move(token));
  }

  // Sort the tokens by expiration time, then prune expired tokens.
  for (auto& [_, cache] : cache_by_geo_) {
    std::sort(cache.begin(), cache.end(),
              [](const BlindSignedAuthToken& a, const BlindSignedAuthToken& b) {
                return a.expiration < b.expiration;
              });
  }
  RemoveExpiredTokens();

  // Record recycled (previously orphaned, unexpired) tokens.
  size_t recycled_count = 0;
  for (const auto& [_, cache] : cache_by_geo_) {
    recycled_count += cache.size();
  }
  if (recycled_count > 0) {
    cache_has_been_filled_ = true;
    Telemetry().RecordTokenCountEvent(
        proxy_layer_, IpProtectionTokenCountEvent::kRecycled, recycled_count);
  }
}

bool IpProtectionTokenManagerImpl::IsAuthTokenAvailable(
    const std::string& geo_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (geo_id == "") {
    return false;
  }

  RemoveExpiredTokens();

  // After `RemoveExpiredTokens()`, any keys for an empty token deque will be
  // removed. Thus, we do not need to check if the deque is empty or not here.
  return cache_by_geo_.contains(geo_id);
}

bool IpProtectionTokenManagerImpl::WasTokenCacheEverFilled() {
  return cache_has_been_filled_;
}

// If this is a good time to request another batch of tokens, do so. This
// method is idempotent, and can be called at any time.
void IpProtectionTokenManagerImpl::MaybeRefillCache() {
  RemoveExpiredTokens();
  if (fetching_auth_tokens_ || !fetcher_ || !ip_protection_core_ ||
      disable_cache_management_for_testing_) {
    return;
  }

  if (!try_get_auth_tokens_after_.is_null() &&
      base::Time::Now() < try_get_auth_tokens_after_) {
    // We must continue to wait before calling `TryGetAuthTokens()` again, so
    // there is nothing we can do to refill the cache at this time. The
    // `next_maybe_refill_cache_` timer is probably already set, but an extra
    // call to `ScheduleMaybeRefillCache()` doesn't hurt.
    ScheduleMaybeRefillCache();
    return;
  }

  if (NeedsRefill(current_geo_id_)) {
    fetching_auth_tokens_ = true;
    tokens_demanded_during_fetch_ = 0;
    VLOG(2) << "IPPATC::MaybeRefillCache calling TryGetAuthTokens";
    fetcher_->TryGetAuthTokens(
        batch_size_, proxy_layer_,
        base::BindOnce(
            &IpProtectionTokenManagerImpl::OnGotAuthTokens,
            weak_ptr_factory_.GetWeakPtr(),
            /*attempt_start_time_for_metrics=*/base::TimeTicks::Now()));
  }

  ScheduleMaybeRefillCache();
}

void IpProtectionTokenManagerImpl::InvalidateTryAgainAfterTime() {
  try_get_auth_tokens_after_ = base::Time();
  ScheduleMaybeRefillCache();
}

void IpProtectionTokenManagerImpl::RecordTokenDemand() {
  if (fetching_auth_tokens_) {
    tokens_demanded_during_fetch_++;
  }
}

std::string IpProtectionTokenManagerImpl::CurrentGeo() const {
  return current_geo_id_;
}

void IpProtectionTokenManagerImpl::SetCurrentGeo(const std::string& geo_id) {
  // Ensuring that a geo change has occurred.
  if (emitted_geo_presence_histogram_before_refill_ && current_geo_id_ != "" &&
      current_geo_id_ != geo_id) {
    Telemetry().GeoChangeTokenPresence(cache_by_geo_.contains(geo_id));
  }

  current_geo_id_ = geo_id;

  // Now that the current geo has been set, the next opportunity to record the
  // "GeoChangeTokenPresence" metric should be taken.
  emitted_geo_presence_histogram_before_refill_ = true;

  MaybeRefillCache();
}

// Schedule the next timed call to `MaybeRefillCache()`. This method is
// idempotent, and may be called at any time.
void IpProtectionTokenManagerImpl::ScheduleMaybeRefillCache() {
  // Early return cases:
  // 1. If currently retrieving tokens, the call will be rescheduled when that
  //    completes, so there is no need to call a refill here.
  // 2. If there is no config getter or config cache, there is nothing to do.
  // 3. If testing requires disabling the cache management.
  if (fetching_auth_tokens_ || !fetcher_ || !ip_protection_core_ ||
      disable_cache_management_for_testing_) {
    next_maybe_refill_cache_.Stop();
    return;
  }

  base::Time now = base::Time::Now();
  base::TimeDelta delay;

  if (NeedsRefill(current_geo_id_)) {
    if (try_get_auth_tokens_after_.is_null()) {
      delay = base::TimeDelta();
    } else {
      delay = try_get_auth_tokens_after_ - now;
    }
  } else {
    auto it = cache_by_geo_.find(current_geo_id_);
    if (it != cache_by_geo_.end() && !it->second.empty()) {
      // Delay refill to when the next token expires.
      delay = it->second.front().expiration - now;
    } else {
      // NeedsRefill returned false, and there are no tokens for the current
      // geo. This happens when current_geo_id_ has not been set yet.
      // Wait for the geo ID to change before attempting to refill again.
      CHECK_EQ(current_geo_id_, "");
      next_maybe_refill_cache_.Stop();
      return;
    }
  }

  if (delay.is_negative()) {
    delay = base::TimeDelta();
  }

  next_maybe_refill_cache_.Start(
      FROM_HERE, delay,
      base::BindOnce(&IpProtectionTokenManagerImpl::MaybeRefillCache,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Returns true if the cache map does not contain the necessary geo or the
// number of tokens in the latest geo is below the low water mark.
bool IpProtectionTokenManagerImpl::NeedsRefill(
    const std::string& geo_id) const {
  if (cache_by_geo_.empty()) {
    return true;
  }

  // There are two states where geo id can be "":
  // 1. The token cache manager was just initialized and has not yet received a
  //    call to SetCurrentGeo.
  // 2. The current geo has been set to "" because there is no available proxy
  //    list and we are falling back to DIRECT. In this case we should not
  //    refill tokens.
  if (geo_id == "") {
    return false;
  }

  auto it = cache_by_geo_.find(geo_id);

  if (it == cache_by_geo_.end()) {
    return true;
  }

  const std::deque<BlindSignedAuthToken>& cache = it->second;
  return cache.size() < cache_low_water_mark_;
}

// Returns true if the cache of the latest geo contains more that enough
// tokens.
// This indicates a possible bad state where new tokens are continually
// being requested "on-demand" due to a geo mismatch between token and proxy
// list signals in `IpProtectionCore`.
bool IpProtectionTokenManagerImpl::IsTokenLimitExceeded(
    const std::string& geo_id) const {
  auto it = cache_by_geo_.find(geo_id);
  if (it == cache_by_geo_.end()) {
    return false;
  }

  const std::deque<BlindSignedAuthToken>& cache = it->second;
  return cache.size() > batch_size_ + cache_low_water_mark_;
}

void IpProtectionTokenManagerImpl::OnGotAuthTokens(
    const base::TimeTicks attempt_start_time_for_metrics,
    std::optional<std::vector<BlindSignedAuthToken>> tokens,
    std::optional<base::Time> try_again_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Failed Call - Short circuit and schedule refill.
  // If `tokens.has_value()` is true, a non-empty list of valid tokens exists.
  if (!tokens.has_value()) {
    fetching_auth_tokens_ = false;
    VLOG(2) << "IPPATC::OnGotAuthTokens back off until " << *try_again_after;
    try_get_auth_tokens_after_ = *try_again_after;

    if (on_try_get_auth_tokens_completed_for_testing_) {
      std::move(on_try_get_auth_tokens_completed_for_testing_).Run();
    }

    ScheduleMaybeRefillCache();
    return;
  }

  // Log is consumed by E2E tests. Please CC potassium-engprod@google.com if you
  // have to change this log.
  VLOG(2) << "IPPATC::OnGotAuthTokens got " << tokens->size()
          << " tokens for proxy " << int(proxy_layer_);
  try_get_auth_tokens_after_ = base::Time();

  RemoveExpiredTokens();

  // Ensure token list is not empty.
  CHECK(!tokens->empty());

  // Randomize the expiration time of the tokens, applying the same "fuzz" to
  // all tokens in the batch.
  if (enable_token_expiration_fuzzing_) {
    base::TimeDelta fuzz_limit = net::features::kIpPrivacyExpirationFuzz.Get();
    base::TimeDelta fuzz =
        base::RandTimeDelta(kMinimumFuzzInterval, fuzz_limit);
    for (auto& token : *tokens) {
      token.expiration -= fuzz;
    }
  }

  std::string geo_id_from_token = GetGeoIdFromGeoHint(tokens->front().geo_hint);

  // Metric should only be recorded under the following conditions:
  // 1. The geo from the token is different from the current geo of the cache.
  // 2. Current geo is not empty which signifies the initial fill of the cache.
  bool has_geo_id_changed = geo_id_from_token != current_geo_id_;
  if (has_geo_id_changed && current_geo_id_ != "") {
    Telemetry().GeoChangeTokenPresence(
        cache_by_geo_.contains(geo_id_from_token));
    emitted_geo_presence_histogram_before_refill_ = false;
  }

  // The latest tokens should be placed into the map of caches.
  if (!cache_by_geo_.contains(geo_id_from_token)) {
    cache_by_geo_.emplace(geo_id_from_token,
                          std::deque<BlindSignedAuthToken>());
  }

  std::deque<BlindSignedAuthToken>& cache = cache_by_geo_[geo_id_from_token];

  // Log the number of tokens successfully fetched.
  Telemetry().RecordTokenCountEvent(
      proxy_layer_, IpProtectionTokenCountEvent::kIssued, tokens->size());
  if (cache_has_been_filled_) {
    Telemetry().TokenDemandDuringBatchGeneration(tokens_demanded_during_fetch_);
  }

  cache.insert(cache.end(), std::make_move_iterator(tokens->begin()),
               std::make_move_iterator(tokens->end()));
  std::sort(cache.begin(), cache.end(),
            [](BlindSignedAuthToken& a, BlindSignedAuthToken& b) {
              return a.expiration < b.expiration;
            });

  // Cache at this point should be filled with tokens at least once.
  cache_has_been_filled_ = true;

  // If a refill is still needed, we do not want to immediately re-request
  // tokens, lest we overwhelm the server. This is unlikely to happen in
  // practice, but exists as a safety check.
  if (NeedsRefill(geo_id_from_token)) {
    try_get_auth_tokens_after_ = base::Time::Now() + kImmediateTokenRefillDelay;
  }

  // Add an extended delay in event of overflow since this could be indicative
  // of a bad state causing a loop.
  if (IsTokenLimitExceeded(geo_id_from_token)) {
    try_get_auth_tokens_after_ = base::Time::Now() + kTokenLimitExceededDelay;
  }

  fetching_auth_tokens_ = false;

  // TODO(abhipatel): Change logic so that external code is not being relied on
  // to update our internal state.
  if (has_geo_id_changed) {
    ip_protection_core_->GeoObserved(geo_id_from_token);
  }

  Telemetry().TokenBatchGenerationComplete(base::TimeTicks::Now() -
                                           attempt_start_time_for_metrics);

  if (on_try_get_auth_tokens_completed_for_testing_) {
    std::move(on_try_get_auth_tokens_completed_for_testing_).Run();
  }

  ScheduleMaybeRefillCache();
}

std::optional<BlindSignedAuthToken> IpProtectionTokenManagerImpl::GetAuthToken(
    const std::string& geo_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoveExpiredTokens();

  std::optional<BlindSignedAuthToken> result;
  size_t tokens_in_cache = 0;
  // Checks to see if the geo is available in the map and then checks if the
  // cache itself is not empty.
  if (auto it = cache_by_geo_.find(geo_id);
      it != cache_by_geo_.end() && !it->second.empty()) {
    tokens_in_cache = it->second.size();
    result.emplace(std::move(it->second.front()));
    it->second.pop_front();
    Telemetry().RecordTokenCountEvent(proxy_layer_,
                                      IpProtectionTokenCountEvent::kSpent, 1);
  }

  Telemetry().GetAuthTokenResultForGeo(
      result.has_value(), cache_by_geo_.empty(), geo_id == current_geo_id_);
  VLOG(2) << "IPPATC::GetAuthToken with " << tokens_in_cache
          << " tokens available";
  MaybeRefillCache();
  return result;
}

// All calls to this function should be accompanied by a call to
// `MaybeRefillCache()`.
void IpProtectionTokenManagerImpl::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now();
  for (auto it = cache_by_geo_.begin(); it != cache_by_geo_.end();) {
    std::deque<BlindSignedAuthToken>& tokens = it->second;
    // Remove expired tokens from each geo. Tokens are sorted and sooner
    // expirations are toward the front of the deque.
    int64_t intial_tokens_expired = tokens_expired_;
    while (!tokens.empty() && tokens.front().expiration <= fresh_after) {
      tokens.pop_front();
      tokens_expired_++;
    }

    // Only emit expired token metric if tokens actually expired.
    int64_t tokens_expired_delta = tokens_expired_ - intial_tokens_expired;
    if (tokens_expired_delta > 0) {
      Telemetry().RecordTokenCountEvent(proxy_layer_,
                                        IpProtectionTokenCountEvent::kExpired,
                                        tokens_expired_delta);
    }

    // A map entry should be removed if the entry contains no tokens and the
    // current geo does not match.
    if (tokens.empty()) {
      it = cache_by_geo_.erase(it);
    } else {
      ++it;
    }
  }
}

void IpProtectionTokenManagerImpl::MeasureTokenRates() {
  auto now = base::TimeTicks::Now();
  auto interval = now - last_token_rate_measurement_;
  auto interval_ms = interval.InMilliseconds();

  auto denominator = base::Hours(1).InMilliseconds();
  if (interval_ms != 0) {
    last_token_rate_measurement_ = now;
    auto expiration_rate = tokens_expired_ * denominator / interval_ms;
    // Entire batches of tokens are likely to expire within a single 5-minute
    // measurement interval. 1024 tokens in 5 minutes is equivalent to 12288
    // tokens per hour, comfortably under 100,000.
    Telemetry().TokenExpirationRate(proxy_layer_, expiration_rate);
  }

  last_token_rate_measurement_ = now;
  tokens_expired_ = 0;
}

void IpProtectionTokenManagerImpl::DisableCacheManagementForTesting(
    base::OnceClosure on_cache_management_disabled) {
  if (fetching_auth_tokens_) {
    // If a `TryGetAuthTokens()` call is underway (due to active cache
    // management), wait for it to finish.
    SetOnTryGetAuthTokensCompletedForTesting(  // IN-TEST
        base::BindOnce(
            &IpProtectionTokenManagerImpl::DisableCacheManagementForTesting,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(on_cache_management_disabled)));
    return;
  }

  // Mark cache management as disabled and reset everything.
  disable_cache_management_for_testing_ = true;
  try_get_auth_tokens_after_ = base::Time();
  cache_by_geo_.clear();
  next_maybe_refill_cache_.Stop();

  std::move(on_cache_management_disabled).Run();
}

void IpProtectionTokenManagerImpl::EnableTokenExpirationFuzzingForTesting(
    bool enable) {
  enable_token_expiration_fuzzing_ = enable;
}

// Call `TryGetAuthTokens()`, which will call
// `on_try_get_auth_tokens_completed_for_testing_` when complete.
void IpProtectionTokenManagerImpl::CallTryGetAuthTokensForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(fetcher_);
  CHECK(on_try_get_auth_tokens_completed_for_testing_);
  fetcher_->TryGetAuthTokens(
      batch_size_, proxy_layer_,
      base::BindOnce(
          &IpProtectionTokenManagerImpl::OnGotAuthTokens,
          weak_ptr_factory_.GetWeakPtr(),
          /*attempt_start_time_for_metrics=*/base::TimeTicks::Now()));
}

}  // namespace ip_protection
