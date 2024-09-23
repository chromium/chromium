// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_cache.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"

namespace safe_browsing {

namespace {

void LogCacheHitOrMiss(bool is_hit) {
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.CacheHit", is_hit);
}
void LogInitialCacheDurationOnSet(base::TimeDelta cache_duration) {
  // The cache is only expected to last a few minutes, but we allow logging up
  // to 1 hour to confirm that there aren't unexpected times.
  base::UmaHistogramLongTimes("SafeBrowsing.HPRT.CacheDuration.InitialOnSet",
                              cache_duration);
}
void LogRemainingCacheDurationOnHit(base::Time expiration_time) {
  // The cache is only expected to last a few minutes, but we allow logging up
  // to 1 hour to confirm that there aren't unexpected times.
  base::UmaHistogramLongTimes("SafeBrowsing.HPRT.CacheDuration.RemainingOnHit",
                              expiration_time - base::Time::Now());
}

}  // namespace

HashRealTimeCache::HashRealTimeCache() = default;

HashRealTimeCache::~HashRealTimeCache() = default;

HashRealTimeCache::FullHashesAndDetails::FullHashesAndDetails() = default;
HashRealTimeCache::FullHashesAndDetails::~FullHashesAndDetails() = default;

std::unordered_map<std::string, std::vector<V5::FullHash>>
HashRealTimeCache::SearchCache(
    const std::set<std::string>& hash_prefixes) const {
  std::unordered_map<std::string, std::vector<V5::FullHash>> results;
  for (const auto& hash_prefix : hash_prefixes) {
    auto cached_result_it = cache_.find(hash_prefix);
    if (cached_result_it != cache_.end() &&
        cached_result_it->second.expiration_time > base::Time::Now()) {
      results[hash_prefix] = cached_result_it->second.full_hash_and_details;
      LogRemainingCacheDurationOnHit(cached_result_it->second.expiration_time);
      LogCacheHitOrMiss(/*is_hit=*/true);
    } else {
      LogCacheHitOrMiss(/*is_hit=*/false);
    }
  }
  return results;
}

void HashRealTimeCache::CacheSearchHashesResponse(
    const std::vector<std::string>& requested_hash_prefixes,
    const std::vector<V5::FullHash>& response_full_hashes,
    const V5::Duration& cache_duration) {
  // First, wipe all the results for the relevant hash prefixes, and set the
  // latest expiry.
  for (const auto& hash_prefix : requested_hash_prefixes) {
    FullHashesAndDetails entry;
    base::TimeDelta cache_duration_time_delta =
        base::Seconds(cache_duration.seconds()) +
        base::Nanoseconds(cache_duration.nanos());
    entry.expiration_time = base::Time::Now() + cache_duration_time_delta;
    cache_[hash_prefix] = entry;
    LogInitialCacheDurationOnSet(cache_duration_time_delta);
  }
  // Then, add all matching and relevant full hashes into the cache. Hash
  // prefixes only sometimes have matching full hashes, so some may remain empty
  // due to the wiping that occurred above.
  for (const auto& fh : response_full_hashes) {
    // Narrow down each full hash's results to just the threat types that are
    // relevant for hash-prefix real-time lookups.
    V5::FullHash full_hash_to_store;
    full_hash_to_store.set_full_hash(fh.full_hash());
    for (const auto& fhd : fh.full_hash_details()) {
      if (hash_realtime_utils::IsHashDetailRelevant(fhd)) {
        auto* fhd_to_store = full_hash_to_store.add_full_hash_details();
        fhd_to_store->set_threat_type(fhd.threat_type());
        for (auto i = 0; i < fhd.attributes_size(); ++i) {
          fhd_to_store->add_attributes(fhd.attributes(i));
        }
      }
    }
    // If none of the threat types were relevant for the full hash, don't store
    // it in the cache.
    if (full_hash_to_store.full_hash_details().empty()) {
      continue;
    }
    // Update the cache with the remaining results for the associated hash
    // prefix.
    auto hash_prefix = hash_realtime_utils::GetHashPrefix(fh.full_hash());
    auto cached_result_it = cache_.find(hash_prefix);
    if (cached_result_it != cache_.end()) {
      cached_result_it->second.full_hash_and_details.push_back(
          full_hash_to_store);
    } else {
      // There should always be a hash prefix associated with the full hash.
      NOTREACHED_IN_MIGRATION();
    }
  }
}

void HashRealTimeCache::ClearExpiredResults() {
  int num_hash_prefixes = cache_.size();
  int num_full_hashes = 0;
  auto it = cache_.begin();
  while (it != cache_.end()) {
    num_full_hashes += it->second.full_hash_and_details.size();
    if (it->second.expiration_time <= base::Time::Now()) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
  base::UmaHistogramCounts10000("SafeBrowsing.HPRT.Cache.HashPrefixCount",
                                num_hash_prefixes);
  base::UmaHistogramCounts10000("SafeBrowsing.HPRT.Cache.FullHashCount",
                                num_full_hashes);
}

}  // namespace safe_browsing
