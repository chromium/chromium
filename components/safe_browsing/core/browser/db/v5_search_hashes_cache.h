// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_SEARCH_HASHES_CACHE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_SEARCH_HASHES_CACHE_H_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"

namespace safe_browsing {

// This class manages the cache for V5 SearchHashes requests.
class V5SearchHashesCache : public KeyedService,
                            public history::HistoryServiceObserver {
 public:
  explicit V5SearchHashesCache(history::HistoryService* history_service);
  V5SearchHashesCache(const V5SearchHashesCache&) = delete;
  V5SearchHashesCache& operator=(const V5SearchHashesCache&) = delete;
  ~V5SearchHashesCache() override;

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

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Returns true if an artificial cached URL has been set.
  static bool has_artificial_cached_url();

  // Resets the artificial cached URL state for testing.
  static void ResetHasArtificialCachedUrlForTesting();

  // This adds a cached verdict for a URL that has artificially been marked as
  // safe or unsafe (depending on `is_unsafe`). This applies to V5 search hashes
  // lookups.
  void CacheArtificialV5SearchHashesLookupVerdict(const std::string& url_spec,
                                                  bool is_unsafe);

 private:
  friend class V5SearchHashesCacheTest;

  // Remove any entries from the cache that are expired. The purpose of this is
  // for memory management.
  void ClearExpiredResults();

  // Schedules the next cleanup task. Runs every 30 minutes.
  void ScheduleNextCleanUp();

  // This adds a cached verdict for a URL that has artificially been marked as
  // unsafe using the command line flag
  // "mark_as_v5_search_hashes_phishing". This applies to V5 search hashes
  // lookups.
  void CacheArtificialUnsafeV5SearchHashesLookupVerdictFromSwitch();

  // Map of hash prefix -> a `FullHashesAndDetails` object, representing the
  // matching unsafe full hashes.
  std::unordered_map<std::string, FullHashesAndDetails> cache_;

  // Timer used to schedule periodic cleanups of expired entries.
  base::OneShotTimer cleanup_timer_;

  // Scoped observation of HistoryService to listen for history deletions.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Set to true if an artificial cached URL has been set for testing.
  static bool has_artificial_cached_url_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_SEARCH_HASHES_CACHE_H_
