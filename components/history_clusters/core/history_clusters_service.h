// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {
class EntityMetadataProvider;
}  // namespace optimization_guide

namespace site_engagement {
class SiteEngagementScoreProvider;
}  // namespace site_engagement

namespace history_clusters {

class HistoryClustersService;

// Clears `HistoryClustersService`'s keyword cache when 1 or more history
// entries are deleted.
class VisitDeletionObserver : public history::HistoryServiceObserver {
 public:
  explicit VisitDeletionObserver(
      HistoryClustersService* history_clusters_service);

  ~VisitDeletionObserver() override;

  // Starts observing a service for history deletions.
  void AttachToHistoryService(history::HistoryService* history_service);

  // history::HistoryServiceObserver
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

 private:
  HistoryClustersService* history_clusters_service_;

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

// This Service provides an API to the History Clusters for UI entry points.
class HistoryClustersService : public base::SupportsUserData,
                               public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDebugMessage(const std::string& message) = 0;
  };

  // Used to track incomplete, unpersisted visits.
  using IncompleteVisitMap =
      std::map<int64_t, IncompleteVisitContextAnnotations>;

  // Use std::unordered_set here because we have ~1000 elements at the 99th
  // percentile, and we do synchronous lookups as the user types in the omnibox.
  using KeywordSet = std::unordered_set<std::u16string>;
  using URLKeywordSet = std::unordered_set<std::string>;

  // `url_loader_factory` is allowed to be nullptr, like in unit tests.
  // In that case, HistoryClustersService will never instantiate a clustering
  // backend that requires it, such as the RemoteClusteringBackend.
  HistoryClustersService(
      const std::string& application_locale,
      history::HistoryService* history_service,
      optimization_guide::EntityMetadataProvider* entity_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      site_engagement::SiteEngagementScoreProvider* engagement_score_provider);
  HistoryClustersService(const HistoryClustersService&) = delete;
  HistoryClustersService& operator=(const HistoryClustersService&) = delete;
  ~HistoryClustersService() override;

  // Gets a weak pointer to this service. Used when UIs want to create a query
  // state object whose lifetime might exceed the service.
  base::WeakPtr<HistoryClustersService> GetWeakPtr();

  // KeyedService:
  void Shutdown() override;

  // Returns true if the Journeys feature is enabled for the current application
  // locale. This is a cached wrapper of `IsJourneysEnabled()` within features.h
  // that's already evaluated against the g_browser_process application locale.
  bool IsJourneysEnabled() const;

  // Used to add and remove observers.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Returns whether observers are registered to notify the debug messages.
  bool ShouldNotifyDebugMessage() const;

  // Notifies the observers of a debug message being available.
  void NotifyDebugMessage(const std::string& message) const;

  // TODO(manukh) `HistoryClustersService` should be responsible for
  // constructing the
  //  `AnnotatedVisit`s rather than exposing these methods which are used by
  //  `HistoryClustersTabHelper` to construct the visits.
  // Gets an `IncompleteVisitContextAnnotations` after DCHECKing it exists; this
  // saves the call sites the effort.
  IncompleteVisitContextAnnotations& GetIncompleteVisitContextAnnotations(
      int64_t nav_id);
  // Gets or creates an `IncompleteVisitContextAnnotations`.
  IncompleteVisitContextAnnotations&
  GetOrCreateIncompleteVisitContextAnnotations(int64_t nav_id);
  // Returns whether an `IncompleteVisitContextAnnotations` exists.
  // TODO(manukh): Merge `HasIncompleteVisitContextAnnotations()` and
  //  `GetIncompleteVisitContextAnnotations()`.
  bool HasIncompleteVisitContextAnnotations(int64_t nav_id);
  // Completes the `IncompleteVisitContextAnnotations` if the expected metrics
  // have been recorded. References retrieved prior will no longer be valid.
  void CompleteVisitContextAnnotationsIfReady(int64_t nav_id);

  // This is a low-level API that doesn't support querying by search terms or
  // de-duplication across multiple batches. Any UI should almost certainly use
  // `QueryClustersState` instead.
  //
  // Returns the freshest clusters created from the user visit history based on
  // `query`, `begin_time`, and `end_time`.
  // - `begin_time` is an inclusive lower bound. In the general case where the
  //   caller wants to traverse to the start of history, `base::Time()` should
  //   be used.
  // - `end_time` is an exclusive upper bound and should be set to
  //   `base::Time()` if the caller wants the newest visits.
  // The returned clusters are sorted in reverse-chronological order based on
  // their highest scoring visit. The visits within each cluster are sorted by
  // score, from highest to lowest.
  //
  // TODO(tommycli): Investigate entirely hiding access to this low-level method
  // behind QueryClustersState.
  void QueryClusters(ClusteringRequestSource clustering_request_source,
                     base::Time begin_time,
                     base::Time end_time,
                     QueryClustersCallback callback,
                     base::CancelableTaskTracker* task_tracker);

  // Removes all visits to the specified URLs in the specified time ranges in
  // `expire_list`. Calls `closure` when done.
  void RemoveVisits(const std::vector<history::ExpireHistoryArgs>& expire_list,
                    base::OnceClosure closure,
                    base::CancelableTaskTracker* task_tracker);

  // Returns true synchronously if `query` matches a cluster keyword. This
  // ignores clusters with only one visit to avoid overtriggering.
  // Note: This depends on the cache state, so this may kick off a cache refresh
  // request while immediately returning false. It's expected that on the next
  // keystroke, the cache may be ready and return true then.
  bool DoesQueryMatchAnyCluster(const std::string& query);

  // Returns true if `url_keyword` matches a URL in a significant cluster. This
  // may kick off a cache refresh while still immediately returning false.
  // `url_keyword` is derived from a given URL by ComputeURLKeywordForLookup().
  // SRP URLs canonicalized by TemplateURLService should be passed in directly.
  bool DoesURLMatchAnyCluster(const std::string& url_keyword);

  // Clears `all_keywords_cache_` and cancels any pending tasks to populate it.
  void ClearKeywordCache();

 private:
  friend class HistoryClustersServiceTestApi;

  // Starts a keyword cache refresh, if necessary.
  void StartKeywordCacheRefresh();

  // This is a callback used for the `QueryClusters()` call from
  // `DoesQueryMatchAnyCluster()`. Accumulates the keywords in `result` within
  // `keyword_accumulator`. If History is not yet exhausted, will request
  // another batch of clusters. Otherwise, will update the keyword cache.
  void PopulateClusterKeywordCache(
      base::ElapsedTimer total_latency_timer,
      base::Time begin_time,
      std::unique_ptr<KeywordSet> keyword_accumulator,
      std::unique_ptr<URLKeywordSet> url_keyword_accumulator,
      KeywordSet* cache,
      URLKeywordSet* url_cache,
      std::vector<history::Cluster> clusters,
      base::Time continuation_end_time);

  // Internally used callback for `QueryClusters()`.
  void OnGotHistoryVisits(ClusteringRequestSource clustering_request_source,
                          base::TimeTicks query_visits_start,
                          QueryClustersCallback callback,
                          std::vector<history::AnnotatedVisit> annotated_visits,
                          base::Time continuation_end_time) const;

  // Runs on UI thread. Internally used callback for `OnGotHistoryVisits()`.
  void OnGotRawClusters(base::Time continuation_end_time,
                        base::TimeTicks cluster_start_time,
                        QueryClustersCallback callback,
                        std::vector<history::Cluster> clusters) const;

  // True if Journeys is enabled based on field trial and locale checks.
  const bool is_journeys_enabled_;

  // Non-owning pointer, but never nullptr.
  history::HistoryService* const history_service_;

  // `VisitContextAnnotations`s are constructed stepwise; they're initially
  // placed in `incomplete_visit_context_annotations_` and saved to the history
  // database once completed (if persistence is enabled).
  IncompleteVisitMap incomplete_visit_context_annotations_;

  // The backend used for clustering. This can be nullptr.
  std::unique_ptr<ClusteringBackend> backend_;

  // In-memory cache of keywords match clusters, so we can query this
  // synchronously as the user types in the omnibox. Also save the timestamp
  // the cache was generated so we can periodically re-generate.
  // TODO(tommycli): Make a smarter mechanism for regenerating the cache.
  KeywordSet all_keywords_cache_;
  URLKeywordSet all_url_keywords_cache_;
  base::Time all_keywords_cache_timestamp_;

  // Like above, but will represent the clusters newer than
  // `all_keywords_cache_timestamp_` I.e., this will contain up to 2 hours of
  // clusters. This can be up to 10 seconds stale. We use a separate cache that
  // can repeatedly be cleared and recreated instead of incrementally adding
  // keywords to `all_keywords_cache_` because doing the latter might:
  //  1) Give a different set of keywords since cluster keywords aren't
  //     necessarily a union of the individual visits' keywords.
  //  2) Exclude keywords since keywords of size-1 clusters are not cached.
  // TODO(manukh) This is a "band aid" fix to missing keywords for recent
  //  visits.
  KeywordSet short_keyword_cache_;
  URLKeywordSet short_url_keywords_cache_;
  base::Time short_keyword_cache_timestamp_;

  base::CancelableTaskTracker cache_query_task_tracker_;

  // A list of observers for this service.
  base::ObserverList<Observer> observers_;

  VisitDeletionObserver visit_deletion_observer_;

  // Weak pointers issued from this factory never get invalidated before the
  // service is destroyed.
  base::WeakPtrFactory<HistoryClustersService> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
