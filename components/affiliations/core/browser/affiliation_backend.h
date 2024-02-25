// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_BACKEND_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_BACKEND_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler_delegate.h"
#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/facet_manager_host.h"

namespace base {
class Clock;
class FilePath;
class SequencedTaskRunner;
class TaskRunner;
class TickClock;
class Time;
}  // namespace base

namespace network {
class NetworkConnectionTracker;
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace affiliations {

class AffiliationDatabase;
class AffiliationFetcherInterface;
class AffiliationFetcherFactory;
class AffiliationFetchThrottler;
class FacetManager;

// The AffiliationBackend is the part of the AffiliationService that
// lives on a background thread suitable for performing blocking I/O. As most
// tasks require I/O, the backend ends up doing most of the work for the
// AffiliationService; the latter being just a thin layer that delegates
// most tasks to the backend.
//
// This class is not thread-safe, but it is fine to construct it on one thread
// and then transfer it to the background thread for the rest of its life.
// Initialize() must be called already on the final (background) thread.
class AffiliationBackend : public FacetManagerHost,
                           public AffiliationFetcherDelegate,
                           public AffiliationFetchThrottlerDelegate {
 public:
  using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

  // Constructs an instance that will use |url_loader_factory| for all
  // network requests, use |task_runner| for asynchronous tasks, and will rely
  // on |time_source| and |time_tick_source| to tell the current time/ticks.
  // Construction is very cheap, expensive steps are deferred to Initialize().
  AffiliationBackend(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* time_source,
      const base::TickClock* time_tick_source);
  AffiliationBackend(const AffiliationBackend&) = delete;
  AffiliationBackend& operator=(const AffiliationBackend&) = delete;
  ~AffiliationBackend() override;

  // Performs the I/O-heavy part of initialization. The database used to cache
  // affiliation information locally will be opened/created at |db_path|.
  void Initialize(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                      pending_url_loader_factory,
                  network::NetworkConnectionTracker* network_connection_tracker,
                  const base::FilePath& db_path);

  // Implementations for methods of the same name in AffiliationService.
  // They are not documented here again. See affiliation_service.h for
  // details:
  void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      StrategyOnCacheMiss cache_miss_strategy,
      AffiliationService::ResultCallback callback,
      const scoped_refptr<base::TaskRunner>& callback_task_runner);
  void Prefetch(const FacetURI& facet_uri, const base::Time& keep_fresh_until);
  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until);
  void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris);
  void TrimCacheForFacetURI(const FacetURI& facet_uri);
  void TrimUnusedCache(std::vector<FacetURI> facet_uris);
  std::vector<GroupedFacets> GetGroupingInfo(
      std::vector<FacetURI> facet_uris) const;
  std::vector<std::string> GetPSLExtensions() const;
  void UpdateAffiliationsAndBranding(const std::vector<FacetURI>& facets,
                                     base::OnceClosure callback);

  // Deletes the cache database file at |db_path|, and all auxiliary files. The
  // database must be closed before calling this.
  static void DeleteCache(const base::FilePath& db_path);

  // Replaces already initialized |fetcher_factory_| implemented by
  // AffiliationFetcherFactoryImpl with a new instance of
  // AffilationFetcherInterface.
  void SetFetcherFactoryForTesting(
      std::unique_ptr<AffiliationFetcherFactory> fetcher_factory);

 private:
  friend class AffiliationBackendTest;
  FRIEND_TEST_ALL_PREFIXES(
      AffiliationBackendTest,
      DiscardCachedDataIfNoLongerNeededWithEmptyAffiliation);

  // Retrieves the affiliation database. This should only be called after
  // Initialize(...).
  AffiliationDatabase& GetAffiliationDatabaseForTesting();

  // Retrieves the FacetManager corresponding to |facet_uri|, creating it and
  // storing it into |facet_managers_| if it did not exist.
  FacetManager* GetOrCreateFacetManager(const FacetURI& facet_uri);

  // Discards cached data corresponding to |affiliated_facets| unless there are
  // FacetManagers that still need the data.
  void DiscardCachedDataIfNoLongerNeeded(
      const AffiliatedFacets& affiliated_facets);

  // Scheduled by RequestNotificationAtTime() to be called back at times when a
  // FacetManager needs to be notified.
  void OnSendNotification(const FacetURI& facet_uri);

  // FacetManagerHost:
  bool ReadAffiliationsAndBrandingFromDatabase(
      const FacetURI& facet_uri,
      AffiliatedFacetsWithUpdateTime* affiliations) override;
  void SignalNeedNetworkRequest() override;
  void RequestNotificationAtTime(const FacetURI& facet_uri,
                                 base::Time time) override;

  // AffiliationFetcherDelegate:
  void OnFetchSucceeded(
      AffiliationFetcherInterface* fetcher,
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) override;
  void OnFetchFailed(AffiliationFetcherInterface* fetcher) override;
  void OnMalformedResponse(AffiliationFetcherInterface* fetcher) override;

  // AffiliationFetchThrottlerDelegate:
  bool OnCanSendNetworkRequest() override;

  // Returns the number of in-memory FacetManagers. Used only for testing.
  size_t facet_manager_count_for_testing() { return facet_managers_.size(); }

  // Reports the |requested_facet_uri_count| in a single fetch; and the elapsed
  // time before the first fetch, and in-between subsequent fetches.
  void ReportStatistics(size_t requested_facet_uri_count);

  // To be called after Initialize() to use |throttler| instead of the default
  // one. Used only for testing.
  void SetThrottlerForTesting(
      std::unique_ptr<AffiliationFetchThrottler> throttler);

  // Ensures that all methods, excluding construction, are called on the same
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<base::Clock> clock_;
  raw_ptr<const base::TickClock> tick_clock_;

  std::unique_ptr<AffiliationFetcherFactory> fetcher_factory_;
  std::unique_ptr<AffiliationDatabase> cache_;
  std::unique_ptr<AffiliationFetcherInterface> fetcher_;
  std::unique_ptr<AffiliationFetchThrottler> throttler_;

  base::Time construction_time_;
  base::Time last_request_time_;

  // Contains a FacetManager for each facet URI that need ongoing attention. To
  // save memory, managers are discarded as soon as they become redundant.
  std::unordered_map<FacetURI, std::unique_ptr<FacetManager>, FacetURIHash>
      facet_managers_;

  base::WeakPtrFactory<AffiliationBackend> weak_ptr_factory_{this};
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_BACKEND_H_
