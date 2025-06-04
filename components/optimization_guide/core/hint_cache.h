// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINT_CACHE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINT_CACHE_H_

#include <string>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "components/optimization_guide/core/memory_hint.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/proto/hints.pb.h"

class GURL;

namespace optimization_guide {
class StoreUpdateData;

using HintLoadedCallback = base::OnceCallback<void(const proto::Hint*)>;

// Contains a set of optimization hints received from the Cacao service. This
// may include hints received from the ComponentUpdater and hints fetched from a
// Cacao Optimization Guide Service API. The availability of hints is queryable
// via host name and full URL. The cache itself consists of a backing store,
// which allows for asynchronous loading of any available host-keyed hint, and
// an MRU host-keyed cache and a url-keyed cache, which can be used to
// synchronously retrieve recently loaded hints keyed by URL or host.
class HintCache {
 public:
  // Construct the HintCache with an optional backing store and max host-keyed
  // cache size. If a backing store is not provided, all hints will only be
  // stored in-memory.
  explicit HintCache(
      base::WeakPtr<OptimizationGuideStore> optimization_guide_store,
      int max_host_keyed_memory_cache_size);

  HintCache(const HintCache&) = delete;
  HintCache& operator=(const HintCache&) = delete;

  ~HintCache();

  // Initializes the backing store contained within the hint cache, if provided,
  // and asynchronously runs the callback after initialization is complete. If
  // |purge_existing_data| is set to true, then the cache will purge any
  // pre-existing data and begin in a clean state.
  void Initialize(bool purge_existing_data, base::OnceClosure callback);

  // Returns a StoreUpdateData. During component processing, hints from the
  // component are moved into the StoreUpdateData. After component
  // processing completes, the component update data is provided to the backing
  // store in UpdateComponentHints() and used to update its component hints. In
  // the case the provided component version is not newer than the store's
  // version, nullptr will be returned by the call.
  std::unique_ptr<StoreUpdateData> MaybeCreateUpdateDataForComponentHints(
      const base::Version& version) const;

  // Returns an UpdateData created by the store to hold updates for fetched
  // hints. No version is needed nor applicable for fetched hints. During
  // processing of the GetHintsResponse, hints are moved into the update data.
  // After processing is complete, the update data is provided to the backing
  // store to update hints. |update_time| specifies when the hints within the
  // created update data will be scheduled to be updated.
  std::unique_ptr<StoreUpdateData> CreateUpdateDataForFetchedHints(
      base::Time update_time) const;

  // Updates the store's component data using the provided StoreUpdateData
  // and asynchronously runs the provided callback after the update finishes.
  void UpdateComponentHints(std::unique_ptr<StoreUpdateData> component_data,
                            base::OnceClosure callback);

  // Process |get_hints_response| to be stored in the hint cache store.
  // |callback| is asynchronously run when the hints are successfully stored or
  // if the store is not available. |update_time| specifies when the hints
  // within |get_hints_response| will need to be updated next. |hosts_fetched|
  // and |urls_fetched| specifies the hosts and URLs for which specific hints
  // were requested to be fetched. It is expected for |this| to keep track of
  // the result, even if a hint was not returned for the URL.
  void UpdateFetchedHints(
      std::unique_ptr<proto::GetHintsResponse> get_hints_response,
      base::Time update_time,
      const base::flat_set<std::string>& hosts_fetched,
      const base::flat_set<GURL>& urls_fetched,
      base::OnceClosure callback);

  // Purges fetched hints from the owned |optimization_guide_store| that have
  // expired.
  void PurgeExpiredFetchedHints();

  // Purges fetched hints from the owned |optimization_guide_store_| and resets
  // both in-memory hint caches.
  void ClearFetchedHints();

  // Purges fetched hints from the owned |optimization_guide_store_| and resets
  // the host-keyed cache.
  void ClearHostKeyedHints();

  // Returns whether the cache has a hint data for |host| locally (whether
  // in the host-keyed cache or persisted on disk).
  bool HasHint(const std::string& host);

  // Requests that hint data for |host| be loaded asynchronously and passed to
  // |callback| if/when loaded.
  void LoadHint(const std::string& host, HintLoadedCallback callback);

  // Returns the update time provided by |hint_store_|, which specifies when the
  // fetched hints within the store are ready to be updated. If |hint_store_| is
  // not initialized, base::Time() is returned.
  base::Time GetFetchedHintsUpdateTime() const;

  // Returns the hint data for |host| if found in the host-keyed cache,
  // otherwise nullptr.
  const proto::Hint* GetHostKeyedHintIfLoaded(const std::string& host);

  // Returns an unepxired hint data for |url| if found in the url-keyed cache,
  // otherwise nullptr. If the hint is expired, it is removed from the URL-keyed
  // cache. Only HTTP/HTTPS URLs without username and password are supported by
  // the URL-keyed cache, if |url| does not meet this criteria, nullptr is
  // returned. The returned hint is not guaranteed to remain valid and will
  // become invalid if any additional URL-keyed hints are added to the cache
  // that evicts the returned hint.
  proto::Hint* GetURLKeyedHint(const GURL& url);

  // Returns true if the url-keyed cache contains an entry for |url|, even if
  // the entry is empty. If a hint exists but is expired, it returns false.
  bool HasURLKeyedEntryForURL(const GURL& url);

  // Removes any URL-keyed hints that are in |urls|.
  void RemoveHintsForURLs(const base::flat_set<GURL>& urls);

  // Removes any host-keyed hints that are in |hosts|. Note that this will also
  // remove any persisted hints from |hint_store()|. |on_success| will be called
  // when the operation completes successfully. If the operation does not
  // complete successfully, the callback will not be run so calling code must
  // not expect it be called in every circumstance.
  void RemoveHintsForHosts(base::OnceClosure on_success,
                           const base::flat_set<std::string>& hosts);

  // Verifies and processes |hints| and moves the ones it supports into
  // |update_data| and caches any valid URL keyed hints.
  //
  // Returns true if there was at least one hint is moved into |update_data|.
  bool ProcessAndCacheHints(
      google::protobuf::RepeatedPtrField<proto::Hint>* hints,
      StoreUpdateData* update_data);

  // Returns whether the persistent hint store owned by this is available.
  bool IsHintStoreAvailable() const;

  // Returns the persistent store for |this|.
  base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store() {
    return optimization_guide_store_;
  }

  // Override |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // Add hint to the URL-keyed cache. For testing only.
  void AddHintForTesting(const GURL& gurl, std::unique_ptr<proto::Hint> hint);

 private:
  using HostKeyedHintCache =
      base::HashingLRUCache<std::string, std::unique_ptr<MemoryHint>>;

  using URLKeyedHintCache =
      base::HashingLRUCache<std::string, std::unique_ptr<MemoryHint>>;

  // Gets the cache key for the URL-keyed hint cache for the URL.
  std::string GetURLKeyedHintCacheKey(const GURL& url) const;

  // The callback run after the store finishes initialization. This then runs
  // the callback initially provided by the Initialize() call.
  void OnStoreInitialized(base::OnceClosure callback);

  // The callback run after the store finishes loading a hint. This adds the
  // loaded hint to |host_keyed_cache_|, potentially purging the least recently
  // used element, and then runs the callback initially provided by the
  // LoadHint() call.
  void OnLoadStoreHint(
      const std::string& host,
      HintLoadedCallback callback,
      const OptimizationGuideStore::EntryKey& store_hint_entry_key,
      std::unique_ptr<MemoryHint> hint);

  // The backing store used with this hint cache. Set during construction. Not
  // owned.
  base::WeakPtr<OptimizationGuideStore> optimization_guide_store_;

  // The cache of host-keyed hints loaded from the store. Maps store
  // EntryKey to Hint proto. This serves two purposes:
  //  1. Allows hints to be requested on navigation and retained in memory until
  //     commit, when they can be synchronously retrieved from the cache.
  //  2. Reduces churn of needing to reload hints from frequently visited sites
  //     multiple times during a session.
  HostKeyedHintCache host_keyed_cache_;

  // The in-memory cache of URL-keyed hints fetched from the remote optimization
  // guide. Maps a full URL to Hint proto. All available URL-keyed hints are
  // maintained within the cache and are not persisted to disk.
  URLKeyedHintCache url_keyed_hint_cache_;

  // The clock used to determine if hints have expired.
  raw_ptr<const base::Clock> clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak ptr factory to get weak pointer of |this|.
  base::WeakPtrFactory<HintCache> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINT_CACHE_H_
