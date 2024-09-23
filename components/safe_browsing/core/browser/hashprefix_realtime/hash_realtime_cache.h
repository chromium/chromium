// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_CACHE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_CACHE_H_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"

namespace safe_browsing {

// This class manages the cache for hash-prefix real-time lookups.
class HashRealTimeCache {
 public:
  HashRealTimeCache();
  HashRealTimeCache(const HashRealTimeCache&) = delete;
  HashRealTimeCache& operator=(const HashRealTimeCache&) = delete;
  ~HashRealTimeCache();

  struct FullHashesAndDetails {
    FullHashesAndDetails();
    ~FullHashesAndDetails();

    // The time at which this cache entry is no longer considered up-to-date.
    base::Time expiration_time;

    // The list of all full hashes (and related info) that start with a
    // particular hash prefix and are known to be unsafe. This vector may be
    // empty if there are no unsafe matches.
    std::vector<V5::FullHash> full_hash_and_details;
  };

  // Returns a map, where the key is a requested hash prefix and the value is
  // the matching result in the cache. If a requested hash prefix was not in the
  // cache (or has expired), then it is not in the returned map.
  std::unordered_map<std::string, std::vector<V5::FullHash>> SearchCache(
      const std::set<std::string>& hash_prefixes) const;

  // Adds the responses to the cache.
  void CacheSearchHashesResponse(
      const std::vector<std::string>& requested_hash_prefixes,
      const std::vector<V5::FullHash>& response_full_hashes,
      const V5::Duration& cache_duration);

  // Remove any entries from the cache that are expired. The purpose of this is
  // for memory management.
  void ClearExpiredResults();

 private:
  friend class HashRealTimeCacheTest;
  friend class VerdictCacheManagerTest;
  // Map of hash prefix -> a |FullHashesAndDetails| object, representing the
  // matching unsafe full hashes.
  std::unordered_map<std::string, FullHashesAndDetails> cache_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_CACHE_H_
