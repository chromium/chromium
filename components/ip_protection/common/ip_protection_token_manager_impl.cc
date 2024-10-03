// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_manager_impl.h"

#include <memory>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
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

// Default Geo used until caching by geo is enabled.
constexpr char kDefaultGeo[] = "EARTH";

}  // namespace

IpProtectionTokenManagerImpl::IpProtectionTokenManagerImpl(
    IpProtectionCore* core,
    IpProtectionConfigGetter* config_getter,
    ProxyLayer proxy_layer,
    bool disable_cache_management_for_testing)
    : batch_size_(net::features::kIpPrivacyAuthTokenCacheBatchSize.Get()),
      cache_low_water_mark_(
          net::features::kIpPrivacyAuthTokenCacheLowWaterMark.Get()),
      enable_token_caching_by_geo_(
          net::features::kIpPrivacyCacheTokensByGeo.Get()),
      config_getter_(config_getter),
      proxy_layer_(proxy_layer),
      ip_protection_core_(core),
      disable_cache_management_for_testing_(
          disable_cache_management_for_testing) {
  // If caching by geo is disabled, the current geo will be resolved to
  // `kDefaultGeo` and should not be modified.
  if (!enable_token_caching_by_geo_) {
    current_geo_id_ = kDefaultGeo;
  }

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

IpProtectionTokenManagerImpl::~IpProtectionTokenManagerImpl() = default;

bool IpProtectionTokenManagerImpl::IsAuthTokenAvailable() {
  return IsAuthTokenAvailable(current_geo_id_);
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
  return cache_by_geo_.contains(enable_token_caching_by_geo_ ? geo_id
                                                             : kDefaultGeo);
}

// If this is a good time to request another batch of tokens, do so. This
// method is idempotent, and can be called at any time.
void IpProtectionTokenManagerImpl::MaybeRefillCache() {
  RemoveExpiredTokens();
  if (fetching_auth_tokens_ || !config_getter_ || !ip_protection_core_ ||
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
    VLOG(2) << "IPPATC::MaybeRefillCache calling TryGetAuthTokens";
    config_getter_->TryGetAuthTokens(
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

std::string IpProtectionTokenManagerImpl::CurrentGeo() const {
  return current_geo_id_;
}

void IpProtectionTokenManagerImpl::SetCurrentGeo(const std::string& geo_id) {
  // If caching by geo is disabled, no further action is needed.
  if (!enable_token_caching_by_geo_) {
    return;
  }

  // Ensuring that a geo change has occurred.
  if (emitted_geo_presence_histogram_before_refill_ && current_geo_id_ != "" &&
      current_geo_id_ != geo_id) {
    Telemetry().GeoChangeTokenPresence(cache_by_geo_.contains(geo_id));
  }

  current_geo_id_ = geo_id;

  // Now that the current geo has been set, the next opportunity to record the
  // "GeoChangeTokenPresence" metric should be taken.
  emitted_geo_presence_histogram_before_refill_ = true;

  if (NeedsRefill(current_geo_id_) && !fetching_auth_tokens_) {
    MaybeRefillCache();
  }
}

// Schedule the next timed call to `MaybeRefillCache()`. This method is
// idempotent, and may be called at any time.
void IpProtectionTokenManagerImpl::ScheduleMaybeRefillCache() {
  // Early return cases:
  // 1. If currently retrieving tokens, the call will be rescheduled when that
  //    completes, so there is no need to call a refill here.
  // 2. If there is no config getter or config cache, there is nothing to do.
  // 3. If testing requires disabling the cache management.
  if (fetching_auth_tokens_ || !config_getter_ || !ip_protection_core_ ||
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
    // Delay refill to when the next token expires.
    delay = cache_by_geo_[current_geo_id_].front().expiration - now;
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
  // 1. The token cache manager was just initialized and has not retrieved any
  // tokens yet but the condition above should not allow this to be reached.
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

  VLOG(2) << "IPPATC::OnGotAuthTokens got " << tokens->size()
          << " tokens for proxy "
          << int(proxy_layer_);
  try_get_auth_tokens_after_ = base::Time();

  RemoveExpiredTokens();

  // Ensure token list is not empty.
  CHECK(!tokens->empty());

  // Randomize the expiration time of the tokens, applying the same "fuzz" to
  // all tokens in the batch.
  if (enable_token_expiration_fuzzing_for_testing_) {
    base::TimeDelta fuzz_limit = net::features::kIpPrivacyExpirationFuzz.Get();
    base::TimeDelta fuzz =
        base::RandTimeDelta(kMinimumFuzzInterval, fuzz_limit);
    for (auto& token : *tokens) {
      token.expiration -= fuzz;
    }
  }

  // TODO(crbug.com/357439021): Refactor so that each TryAuthTokensCallback
  // contains a single `geo_hint`.
  std::string geo_id_from_token =
      enable_token_caching_by_geo_
          ? GetGeoIdFromGeoHint(tokens->front().geo_hint)
          : kDefaultGeo;

  // Metric should only be recorded under the following conditions:
  // 1. Token caching by geo is enabled.
  // 2. The geo from the token is different from the current geo of the cache.
  // 2. Current geo is not empty which signifies the initial fill of the cache.
  bool has_geo_id_changed = geo_id_from_token != current_geo_id_;
  if (enable_token_caching_by_geo_ && has_geo_id_changed &&
      current_geo_id_ != "") {
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

  cache.insert(cache.end(), std::make_move_iterator(tokens->begin()),
               std::make_move_iterator(tokens->end()));
  std::sort(cache.begin(), cache.end(),
            [](BlindSignedAuthToken& a, BlindSignedAuthToken& b) {
              return a.expiration < b.expiration;
            });

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
  if (enable_token_caching_by_geo_ && has_geo_id_changed) {
    ip_protection_core_->GeoObserved(geo_id_from_token);
  }

  Telemetry().TokenBatchGenerationComplete(base::TimeTicks::Now() -
                                           attempt_start_time_for_metrics);

  if (on_try_get_auth_tokens_completed_for_testing_) {
    std::move(on_try_get_auth_tokens_completed_for_testing_).Run();
  }

  ScheduleMaybeRefillCache();
}

std::optional<BlindSignedAuthToken>
IpProtectionTokenManagerImpl::GetAuthToken() {
  return GetAuthToken(current_geo_id_);
}

std::optional<BlindSignedAuthToken> IpProtectionTokenManagerImpl::GetAuthToken(
    const std::string& geo_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoveExpiredTokens();

  std::optional<BlindSignedAuthToken> result;
  size_t tokens_in_cache = 0;
  // Checks to see if the geo is available in the map and then checks if the
  // cache itself is not empty.
  if (auto it = cache_by_geo_.find(enable_token_caching_by_geo_ ? geo_id
                                                                : kDefaultGeo);
      it != cache_by_geo_.end() && !it->second.empty()) {
    tokens_in_cache = it->second.size();
    result.emplace(std::move(it->second.front()));
    it->second.pop_front();
    tokens_spent_++;
  }

  Telemetry().GetAuthTokenResultForGeo(
      result.has_value(), enable_token_caching_by_geo_, cache_by_geo_.empty(),
      geo_id == current_geo_id_);
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
    while (!tokens.empty() && tokens.front().expiration <= fresh_after) {
      tokens.pop_front();
      tokens_expired_++;
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

    auto spend_rate = tokens_spent_ * denominator / interval_ms;
    // A maximum of 1000 would correspond to a spend rate of about 16/min,
    // which is higher than we expect to see.
    Telemetry().TokenSpendRate(proxy_layer_, spend_rate);

    auto expiration_rate = tokens_expired_ * denominator / interval_ms;
    // Entire batches of tokens are likely to expire within a single 5-minute
    // measurement interval. 1024 tokens in 5 minutes is equivalent to 12288
    // tokens per hour, comfortably under 100,000.
    Telemetry().TokenExpirationRate(proxy_layer_, expiration_rate);
  }

  last_token_rate_measurement_ = now;
  tokens_spent_ = 0;
  tokens_expired_ = 0;
}

void IpProtectionTokenManagerImpl::DisableCacheManagementForTesting(
    base::OnceClosure on_cache_management_disabled) {
  disable_cache_management_for_testing_ = true;
  ScheduleMaybeRefillCache();

  if (fetching_auth_tokens_) {
    // If a `TryGetAuthTokens()` call is underway (due to active cache
    // management), wait for it to finish.
    SetOnTryGetAuthTokensCompletedForTesting(  // IN-TEST
        std::move(on_cache_management_disabled));
    return;
  }
  std::move(on_cache_management_disabled).Run();
}

void IpProtectionTokenManagerImpl::EnableTokenExpirationFuzzingForTesting(
    bool enable) {
  enable_token_expiration_fuzzing_for_testing_ = enable;
}

// Call `TryGetAuthTokens()`, which will call
// `on_try_get_auth_tokens_completed_for_testing_` when complete.
void IpProtectionTokenManagerImpl::CallTryGetAuthTokensForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config_getter_);
  CHECK(on_try_get_auth_tokens_completed_for_testing_);
  config_getter_->TryGetAuthTokens(
      batch_size_, proxy_layer_,
      base::BindOnce(
          &IpProtectionTokenManagerImpl::OnGotAuthTokens,
          weak_ptr_factory_.GetWeakPtr(),
          /*attempt_start_time_for_metrics=*/base::TimeTicks::Now()));
}

}  // namespace ip_protection
