// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/visit_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/query_parser/query_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace history_clusters {

// This Service is the API for UIs to fetch Chrome Memories.
class HistoryClustersService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMemoriesDebugMessage(const std::string& message) = 0;
  };

  // The result data returned by `QueryClusters()`.
  struct QueryClustersResult {
    QueryClustersResult();
    ~QueryClustersResult();
    QueryClustersResult(const QueryClustersResult&);

    std::vector<history::Cluster> clusters;
    base::Time continuation_end_time;
  };
  using QueryClustersCallback = base::OnceCallback<void(QueryClustersResult)>;

  // Used to track incomplete, unpersisted visits.
  using IncompleteVisitMap =
      std::map<int64_t, IncompleteVisitContextAnnotations>;

  // `url_loader_factory` is allowed to be nullptr, like in unit tests.
  // In that case, HistoryClustersService will never instantiate a clustering
  // backend that requires it, such as the RemoteClusteringBackend.
  explicit HistoryClustersService(
      history::HistoryService* history_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  HistoryClustersService(const HistoryClustersService&) = delete;
  HistoryClustersService& operator=(const HistoryClustersService&) = delete;
  ~HistoryClustersService() override;

  // KeyedService:
  void Shutdown() override;

  // Used to add and remove observers.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

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
  // `query`, `end_time`, and `max_count`. `end_time` is an exclusive upper
  // bound and should be set to `base::Time()` if the caller wants everything.
  // The returned clusters are sorted in reverse-chronological order based on
  // their highest scoring visit. The visits within each cluster are sorted by
  // score, from highest to lowest.
  void QueryClusters(const std::string& query,
                     base::Time end_time,
                     size_t max_count,
                     QueryClustersCallback callback,
                     base::CancelableTaskTracker* task_tracker);

  // Removes all visits to the specified URLs in the specified time ranges in
  // `expire_list`. Calls `closure` when done.
  void RemoveVisits(const std::vector<history::ExpireHistoryArgs>& expire_list,
                    base::OnceClosure closure,
                    base::CancelableTaskTracker* task_tracker);

  // Returns true synchronously if `query` matches a cluster keyword.
  // Note: This depends on the cache state, so this may kick off a cache refresh
  // request while immediately returning false. It's expected that on the next
  // keystroke, the cache may be ready and return true then.
  bool DoesQueryMatchAnyCluster(const std::string& query);

 private:
  friend class HistoryClustersServiceTestApi;

  // This is a callback used for the `QueryClusters()` call from
  // `DoesQueryMatchAnyCluster()`. Populates the cluster keyword cache from the
  // keywords in `clusters`.
  void PopulateClusterKeywordCache(QueryClustersResult result);

  // Internally used callback for `QueryClusters()`.
  void OnGotHistoryVisits(const std::string& query,
                          QueryClustersCallback callback,
                          std::vector<history::AnnotatedVisit> annotated_visits,
                          base::Time continuation_end_time) const;

  // Internally used callback for `OnGotHistoryVisits()`.
  void OnGotClusters(const std::string& query,
                     base::Time continuation_end_time,
                     QueryClustersCallback callback,
                     const std::vector<history::Cluster>& clusters) const;

  // `VisitContextAnnotations`s are constructed stepwise; they're initially
  // placed in `incomplete_visit_context_annotations_` and saved to the history
  // database once completed (if persistence is enabled).
  IncompleteVisitMap incomplete_visit_context_annotations_;

  // Non-owning pointer, but never nullptr.
  history::HistoryService* const history_service_;

  // The backend used for clustering. This can be nullptr.
  std::unique_ptr<ClusteringBackend> backend_;

  // In-memory cache of keywords match clusters, so we can query this
  // synchronously as the user types in the omnibox. Also save the timestamp
  // the cache was generated so we can periodically re-generate.
  // TODO(tommycli): Make a smarter mechanism for regenerating the cache.
  query_parser::QueryWordVector all_keywords_cache_;
  base::Time all_keywords_cache_timestamp_;
  base::CancelableTaskTracker cache_query_task_tracker_;

  // A list of observers for this service.
  base::ObserverList<Observer> observers_;

  // Used to asyncly call into `backend_` after async history request. This can
  // be nullptr.
  std::unique_ptr<base::WeakPtrFactory<ClusteringBackend>>
      backend_weak_factory_;
  // Weak pointers issued from this factory never get invalidated before the
  // service is destroyed.
  base::WeakPtrFactory<HistoryClustersService> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_H_
