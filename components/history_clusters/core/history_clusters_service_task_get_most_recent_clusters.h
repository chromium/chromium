// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"

namespace history {
class HistoryService;
}  // namespace history

namespace history_clusters {

class HistoryClustersService;

// `HistoryClustersServiceTaskGetMostRecentClusters` 1st gets newly generated
// clusters from the clustering backend using unclustered visits from the
// history backend. Then, once the unclustered visits are exhausted, it switches
// to getting persisted clusters from the history backend.
// It is an extension of `HistoryClustersService`; rather than pollute the
// latter's namespace with a bunch of callbacks, this class groups those
// callbacks.
class HistoryClustersServiceTaskGetMostRecentClusters
    : public HistoryClustersServiceTask {
 public:
  HistoryClustersServiceTaskGetMostRecentClusters(
      base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
      const IncompleteVisitMap incomplete_visit_context_annotations,
      ClusteringBackend* const backend,
      history::HistoryService* const history_service,
      ClusteringRequestSource clustering_request_source,
      base::Time begin_time,
      QueryClustersContinuationParams continuation_params,
      bool recluster,
      QueryClustersCallback callback);
  ~HistoryClustersServiceTaskGetMostRecentClusters() override;

 private:
  // When there remain unclustered visits, cluster them (possibly in combination
  // with clustered visits) and return the newly created clusters:
  //   Start() ->
  //   OnGotAnnotatedVisitsToCluster() ->
  //   OnGotModelClusters()
  // But when unclustered visits are or were exhausted, return persisted
  // clusters:
  //   Start() ->
  //   [optional] OnGotAnnotatedVisitsToCluster() ->
  //   ReturnMostRecentPersistedClusters() ->
  //   OnGotMostRecentPersistedClusters()

  // Invoked during construction. Will asyncly request annotated visits from
  // `GetAnnotatedVisitsToCluster`. May instead asyncly request persisted
  // clusters if there's no `ClusteringBackend` or all visits are exhausted.
  void Start();

  // Invoked after `Start()` asyncly fetches annotated visits. Will asyncly
  // request clusters from `ClusteringBackend`. May instead asyncly request
  // persisted clusters if no annotated visits were fetched
  void OnGotAnnotatedVisitsToCluster(
      // Unused because clusters aren't persisted in this flow.
      std::vector<int64_t> old_clusters_unused,
      std::vector<history::AnnotatedVisit> annotated_visits,
      QueryClustersContinuationParams continuation_params);

  // Invoked after `OnGotAnnotatedVisitsToCluster()` asyncly obtains clusters.
  // Will syncly invoke `callback_`.
  void OnGotModelClusters(QueryClustersContinuationParams continuation_params,
                          std::vector<history::Cluster> clusters);

  // Invoked syncly when there are no unclustered visits to cluster. Will
  // asyncly request existing (i.e. persisted) clusters from `HistoryService`.
  void ReturnMostRecentPersistedClusters(base::Time exclusive_max_time);

  // Invoked after `ReturnMostRecentPersistedClusters()` asyncly fetches
  // clusters. Will syncly invoke `callback_`.
  void OnGotMostRecentPersistedClusters(std::vector<history::Cluster> clusters);

  // Never nullptr.
  base::WeakPtr<HistoryClustersService> weak_history_clusters_service_;
  const IncompleteVisitMap incomplete_visit_context_annotations_;
  // Non-owning pointer, but never nullptr.
  ClusteringBackend* const backend_;
  // Non-owning pointer, but never nullptr.
  history::HistoryService* const history_service_;

  // Used to make requests to `ClusteringBackend`.
  ClusteringRequestSource clustering_request_source_;

  // Used to make requests to `GetAnnotatedVisitsToCluster` and
  // `HistoryService`.
  base::Time begin_time_;
  QueryClustersContinuationParams continuation_params_;
  bool recluster_;
  base::CancelableTaskTracker task_tracker_;

  // Invoked after either `OnGotModelClusters()` or
  // `OnGotMostRecentPersistedClusters()`.
  QueryClustersCallback callback_;

  // When `Start()` kicked off the request to fetch visits to cluster.
  base::TimeTicks get_annotated_visits_to_cluster_start_time_;
  // When `OnGotAnnotatedVisitsToCluster()` kicked off the request to cluster
  // the visits.
  base::TimeTicks get_model_clusters_start_time_;
  // When `ReturnMostRecentPersistedClusters()` kicked off the request to get
  // persisted clusters.
  base::TimeTicks get_most_recent_persisted_clusters_start_time_;

  // Used for async callbacks.
  base::WeakPtrFactory<HistoryClustersServiceTaskGetMostRecentClusters>
      weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_H_
