// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
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
#include "components/history_clusters/core/context_clusterer_history_service_observer.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;
class TemplateURLService;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace site_engagement {
class SiteEngagementScoreProvider;
}  // namespace site_engagement

namespace history_clusters {

class ClusteringBackend;
class HistoryClustersService;
class HistoryClustersServiceTask;

// This Service provides an API to the History Clusters for UI entry points.
class HistoryClustersService : public base::SupportsUserData,
                               public KeyedService,
                               public history::HistoryServiceObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDebugMessage(const std::string& message) = 0;
  };

  // Use std::unordered_map here because we have ~1000 elements at the 99th
  // percentile, and we do synchronous lookups as the user types in the omnibox.
  using KeywordMap =
      std::unordered_map<std::u16string, history::ClusterKeywordData>;

  // `url_loader_factory` is allowed to be nullptr, like in unit tests.
  HistoryClustersService(
      const std::string& application_locale,
      history::HistoryService* history_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
      TemplateURLService* template_url_service,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      PrefService* pref_service);
  HistoryClustersService(const HistoryClustersService&) = delete;
  HistoryClustersService& operator=(const HistoryClustersService&) = delete;
  ~HistoryClustersService() override;

  // Gets a weak pointer to this service. Used when UIs want to create a query
  // state object whose lifetime might exceed the service.
  base::WeakPtr<HistoryClustersService> GetWeakPtr();

  // KeyedService:
  void Shutdown() override;

  // Returns true if the Journeys feature is enabled both by feature flag AND
  // by the user pref / policy value. Virtual for testing.
  virtual bool IsJourneysEnabledAndVisible() const;

  // Returns true if the Journeys feature is enabled by feature flag, but
  // ignores the pref / policy value.
  bool is_journeys_feature_flag_enabled() const {
    return is_journeys_feature_flag_enabled_;
  }

  // Returns true if the Journeys use of Images is enabled.
  static bool IsJourneysImagesEnabled();

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

  // Returns the freshest clusters created from the user visit history based on
  // `query`, `filter_params`, `begin_time`, and `continuation_params`.
  // - `filter_params` represents how the caller wants the clusters to be
  // filtered.
  // - `begin_time` is an inclusive lower bound. In the general case where the
  //   caller wants to traverse to the start of history, `base::Time()` should
  //   be used.
  // - `continuation_params` represents where the previous request left off. It
  //   should be set to the default initialized
  //   `QueryClustersContinuationParams`
  //   if the caller wants the newest visits.
  // - `recluster`, if true, forces reclustering as if
  //   `persist_clusters_in_history_db` were false.
  // The caller is responsible for checking `IsJourneysEnabled()` before calling
  // this method. Virtual for testing.
  virtual std::unique_ptr<HistoryClustersServiceTask> QueryClusters(
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      base::Time begin_time,
      QueryClustersContinuationParams continuation_params,
      bool recluster,
      QueryClustersCallback callback);

  // Entrypoint to the `HistoryClustersServiceTaskUpdateClusters`. Updates the
  // persisted clusters in the history DB and invokes `callback` when done.
  void UpdateClusters();

  // Returns matched keyword data from cache synchronously if `query` matches a
  // cluster keyword. This ignores clusters with only one visit to avoid
  // overtriggering. Note: This depends on the cache state, so this may kick off
  // a cache refresh request while immediately returning null data. It's
  // expected that on the next keystroke, the cache may be ready and return the
  // matched keyword data then.
  std::optional<history::ClusterKeywordData> DoesQueryMatchAnyCluster(
      const std::string& query);

  // Prints the keyword bag state to the log messages. For example, a button on
  // chrome://history-clusters-internals triggers this.
  void PrintKeywordBagStateToLogMessage() const;

  void set_keyword_cache_refresh_callback_for_testing(
      base::OnceClosure&& closure) {
    keyword_cache_refresh_callback_for_testing_ = std::move(closure);
  }

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& visit_row) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class HistoryClustersServiceTestApi;
  friend class HistoryClustersServiceTest;

  // Invokes `UpdateClusters()` after a short delay, then again periodically.
  // E.g., might invoke `UpdateClusters()` initially 5 minutes after startup,
  // then every 1 hour afterwards.
  void RepeatedlyUpdateClusters();

  // Starts a keyword cache refresh, if necessary.
  // TODO(manukh): `StartKeywordCacheRefresh()` and
  //  `PopulateClusterKeywordCache()` should be encapsulated into their own task
  //  to avoid cluttering `HistoryClusterService` with their callbacks. Similar
  //  to the `HistoryClustersServiceTaskGetMostRecentClusters` and
  //  `HistoryClustersServiceTaskUpdateClusters` tasks.
  void StartKeywordCacheRefresh();

  // This is a callback used for the `QueryClusters()` call from
  // `DoesQueryMatchAnyCluster()`. Accumulates the keywords in `result` within
  // `keyword_accumulator`. If History is not yet exhausted, will request
  // another batch of clusters. Otherwise, will update the keyword cache.
  void PopulateClusterKeywordCache(
      base::ElapsedTimer total_latency_timer,
      base::Time begin_time,
      std::unique_ptr<KeywordMap> keyword_accumulator,
      KeywordMap* cache,
      std::vector<history::Cluster> clusters,
      QueryClustersContinuationParams continuation_params);

  // Clears `all_keywords_cache_` and cancels any pending tasks to populate it.
  void ClearKeywordCache();

  // Reads the "all keywords" and short keyword caches from prefs and
  // deserializes them.
  void LoadCachesFromPrefs();
  // Serializes and writes the short keyword cache to prefs.
  void WriteShortCacheToPrefs();
  // Serializes and writes the "all keywords" cache to prefs.
  void WriteAllCacheToPrefs();

  // Whether keyword caches should persisted via the pref service.
  const bool persist_caches_to_prefs_;

  // True if Journeys is enabled based on feature flag and locale checks.
  // But critically, this does NOT check the pref or policy value to see if
  // either the user or Enterprise has disabled Journeys.
  const bool is_journeys_feature_flag_enabled_;

  // Non-owning pointer, but never nullptr.
  history::HistoryService* const history_service_;

  // `VisitContextAnnotations`s are constructed stepwise; they're initially
  // placed in `incomplete_visit_context_annotations_` and saved to the history
  // database once completed (if persistence is enabled).
  IncompleteVisitMap incomplete_visit_context_annotations_;

  // The backend used for clustering. Never nullptr.
  std::unique_ptr<ClusteringBackend> backend_;

  // In-memory cache of keywords match clusters, so we can query this
  // synchronously as the user types in the omnibox. Also save the timestamp
  // the cache was generated so we can periodically re-generate.
  // TODO(tommycli): Make a smarter mechanism for regenerating the cache.
  KeywordMap all_keywords_cache_;
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
  KeywordMap short_keyword_cache_;
  base::Time short_keyword_cache_timestamp_;

  // Closure to signal that the keyword bag has been refreshed for testing.
  // Used only for unit tests.
  base::OnceClosure keyword_cache_refresh_callback_for_testing_;

  // Tracks the current keyword task. Will be `nullptr` or
  // `cache_keyword_query_task_.Done()` will be true if there is no ongoing
  // task.
  std::unique_ptr<HistoryClustersServiceTask> cache_keyword_query_task_;

  // Tracks the current update task. Will be `nullptr` or
  // `update_clusters_task_.Done()` will be true if there is no ongoing task.
  std::unique_ptr<HistoryClustersServiceTask> update_clusters_task_;

  // The time of the last `UpdateClusters()` call. Used for logging and to limit
  // requests when `persist_on_query` is enabled.
  base::ElapsedTimer update_clusters_timer_;

  // Whether a synced visit was received since the last `UpdateClusters()` call.
  // Used to determine whether the full set of persisted clusters needs to be
  // iterated through when updating cluster triggerability. Always set this to
  // true at the beginning of the session, so anything that happened at browser
  // close gets picked up.
  bool received_synced_visit_since_last_update_ = true;

  // A list of observers for this service.
  base::ObserverList<Observer> observers_;

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  std::unique_ptr<ContextClustererHistoryServiceObserver>
      context_clusterer_observer_;

  // Used to store keyword caches across restarts.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Weak pointers issued from this factory never get invalidated before the
  // service is destroyed.
  base::WeakPtrFactory<HistoryClustersService> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
