// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {

namespace {

// TODO(crbug.com/362791941): Rename HPRT logs.
void LogCacheHitOrMiss(bool is_hit) {
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    base::UmaHistogramBoolean("SafeBrowsing.V5Cache.CacheHit", is_hit);
  } else {
    base::UmaHistogramBoolean("SafeBrowsing.HPRT.CacheHit", is_hit);
  }
}
void LogInitialCacheDurationOnSet(base::TimeDelta cache_duration) {
  // The cache is only expected to last a few minutes, but we allow logging up
  // to 1 hour to confirm that there aren't unexpected times.
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    base::UmaHistogramLongTimes(
        "SafeBrowsing.V5Cache.CacheDuration.InitialOnSet", cache_duration);
  } else {
    base::UmaHistogramLongTimes("SafeBrowsing.HPRT.CacheDuration.InitialOnSet",
                                cache_duration);
  }
}
void LogRemainingCacheDurationOnHit(base::Time expiration_time) {
  // The cache is only expected to last a few minutes, but we allow logging up
  // to 1 hour to confirm that there aren't unexpected times.
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    base::UmaHistogramLongTimes(
        "SafeBrowsing.V5Cache.CacheDuration.RemainingOnHit",
        expiration_time - base::Time::Now());
  } else {
    base::UmaHistogramLongTimes(
        "SafeBrowsing.HPRT.CacheDuration.RemainingOnHit",
        expiration_time - base::Time::Now());
  }
}

}  // namespace

// static
bool V5SearchHashesCache::has_artificial_cached_url_ = false;

// static
bool V5SearchHashesCache::has_artificial_cached_url() {
  return has_artificial_cached_url_;
}

// static
void V5SearchHashesCache::ResetHasArtificialCachedUrlForTesting() {
  has_artificial_cached_url_ = false;
}

V5SearchHashesCache::V5SearchHashesCache(
    history::HistoryService* history_service) {
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  ScheduleNextCleanUp();
  CacheArtificialUnsafeV5SearchHashesLookupVerdictFromSwitch();
}

V5SearchHashesCache::~V5SearchHashesCache() = default;

V5SearchHashesCache::FullHashesAndDetails::FullHashesAndDetails() = default;
V5SearchHashesCache::FullHashesAndDetails::~FullHashesAndDetails() = default;

std::unordered_map<std::string, std::vector<V5::FullHash>>
V5SearchHashesCache::SearchCache(
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

void V5SearchHashesCache::CacheSearchHashesResponse(
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
      if (base::FeatureList::IsEnabled(kLocalListsUseSBv5) ||
          hash_realtime_utils::IsHashDetailRelevant(fhd)) {
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
    auto hash_prefix = SBProtocolManagerUtil::GetHashPrefix(fh.full_hash());
    auto cached_result_it = cache_.find(hash_prefix);
    if (cached_result_it != cache_.end()) {
      cached_result_it->second.full_hash_and_details.push_back(
          full_hash_to_store);
    } else {
      // There should always be a hash prefix associated with the full hash.
      NOTREACHED();
    }
  }
}

void V5SearchHashesCache::Shutdown() {
  history_service_observation_.Reset();
  cleanup_timer_.Stop();
  cache_.clear();
}

void V5SearchHashesCache::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    return;
  }

  if (deletion_info.IsAllHistory()) {
    cache_.clear();
    return;
  }

  // If any hash prefix of a cleared URL is found, wipe it from the cache.
  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    if (!row.url().is_valid()) {
      continue;
    }
    std::vector<FullHashStr> full_hashes;
    SBProtocolManagerUtil::UrlToFullHashes(row.url(), &full_hashes);
    for (const auto& full_hash : full_hashes) {
      std::string hash_prefix = SBProtocolManagerUtil::GetHashPrefix(full_hash);
      cache_.erase(hash_prefix);
    }
  }
}

void V5SearchHashesCache::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  CHECK(history_service_observation_.IsObservingSource(history_service));
  history_service_observation_.Reset();
}

void V5SearchHashesCache::ClearExpiredResults() {
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
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    base::UmaHistogramCounts10000("SafeBrowsing.V5Cache.HashPrefixCount",
                                  num_hash_prefixes);
    base::UmaHistogramCounts10000("SafeBrowsing.V5Cache.FullHashCount",
                                  num_full_hashes);
  } else {
    base::UmaHistogramCounts10000("SafeBrowsing.HPRT.Cache.HashPrefixCount",
                                  num_hash_prefixes);
    base::UmaHistogramCounts10000("SafeBrowsing.HPRT.Cache.FullHashCount",
                                  num_full_hashes);
  }
}

void V5SearchHashesCache::ScheduleNextCleanUp() {
  cleanup_timer_.Stop();
  cleanup_timer_.Start(FROM_HERE, base::Minutes(30),
                       base::BindOnce(
                           [](V5SearchHashesCache* cache) {
                             cache->ClearExpiredResults();
                             cache->ScheduleNextCleanUp();
                           },
                           base::Unretained(this)));
}

void V5SearchHashesCache::CacheArtificialV5SearchHashesLookupVerdict(
    const std::string& url_spec,
    bool is_unsafe) {
  if (url_spec.empty()) {
    return;
  }

  GURL artificial_unsafe_url(url_spec);
  if (!artificial_unsafe_url.is_valid()) {
    return;
  }

  has_artificial_cached_url_ = true;

  std::vector<FullHashStr> full_hashes;
  SBProtocolManagerUtil::UrlToFullHashes(artificial_unsafe_url, &full_hashes);
  std::vector<std::string> hash_prefixes;
  for (const auto& full_hash : full_hashes) {
    auto hash_prefix = SBProtocolManagerUtil::GetHashPrefix(full_hash);
    hash_prefixes.emplace_back(hash_prefix);
  }
  FullHashStr sample_full_hash = full_hashes[0];
  V5::FullHash full_hash_object;
  full_hash_object.set_full_hash(sample_full_hash);
  if (is_unsafe) {
    auto* details = full_hash_object.add_full_hash_details();
    details->set_threat_type(V5::ThreatType::SOCIAL_ENGINEERING);
  }
  V5::Duration cache_duration;
  cache_duration.set_seconds(3000);
  CacheSearchHashesResponse(hash_prefixes, {full_hash_object}, cache_duration);
}

void V5SearchHashesCache::
    CacheArtificialUnsafeV5SearchHashesLookupVerdictFromSwitch() {
  std::string phishing_url_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kArtificialCachedV5SearchHashesVerdictFlag);
  CacheArtificialV5SearchHashesLookupVerdict(phishing_url_string,
                                             /*is_unsafe=*/true);
}

}  // namespace safe_browsing
