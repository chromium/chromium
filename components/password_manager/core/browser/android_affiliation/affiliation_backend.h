// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_BACKEND_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetch_throttler_delegate.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/android_affiliation/facet_manager_host.h"

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
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace password_manager {

class AffiliationDatabase;
class AffiliationFetcher;
class AffiliationFetchThrottler;
class FacetManager;

// The AffiliationBackend is the part of the AffiliationService that lives on a
// background thread suitable for performing blocking I/O. As most tasks require
// I/O, the backend ends up doing most of the work for the AffiliationService;
// the latter being just a thin layer that delegates most tasks to the backend.
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
  ~AffiliationBackend() override;

  // Performs the I/O-heavy part of initialization. The database used to cache
  // affiliation information locally will be opened/created at |db_path|.
  void Initialize(std::unique_ptr<network::SharedURLLoaderFactoryInfo>
                      url_loader_factory_info,
                  network::NetworkConnectionTracker* network_connection_tracker,
                  const base::FilePath& db_path);

  // Implementations for methods of the same name in AffiliationService. They
  // are not documented here again. See affiliation_service.h for details:
  void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      StrategyOnCacheMiss cache_miss_strategy,
      AffiliationService::ResultCallback callback,
      const scoped_refptr<base::TaskRunner>& callback_task_runner);
  void Prefetch(const FacetURI& facet_uri, const base::Time& keep_fresh_until);
  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until);
  void TrimCacheForFacetURI(const FacetURI& facet_uri);

  // Deletes the cache database file at |db_path|, and all auxiliary files. The
  // database must be closed before calling this.
  static void DeleteCache(const base::FilePath& db_path);

 private:
  friend class AffiliationBackendTest;
  FRIEND_TEST_ALL_PREFIXES(
      AffiliationBackendTest,
      DiscardCachedDataIfNoLongerNeededWithEmptyAffiliation);

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
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) override;
  void OnFetchFailed() override;
  void OnMalformedResponse() override;

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
  base::Clock* clock_;
  const base::TickClock* tick_clock_;

  std::unique_ptr<AffiliationDatabase> cache_;
  std::unique_ptr<AffiliationFetcher> fetcher_;
  std::unique_ptr<AffiliationFetchThrottler> throttler_;

  base::Time construction_time_;
  base::Time last_request_time_;

  // Contains a FacetManager for each facet URI that need ongoing attention. To
  // save memory, managers are discarded as soon as they become redundant.
  std::unordered_map<FacetURI, std::unique_ptr<FacetManager>, FacetURIHash>
      facet_managers_;

  base::WeakPtrFactory<AffiliationBackend> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AffiliationBackend);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_BACKEND_H_
