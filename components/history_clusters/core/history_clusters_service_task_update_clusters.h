// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_UPDATE_CLUSTERS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_UPDATE_CLUSTERS_H_

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
}

namespace history_clusters {

class HistoryClustersService;

// `HistoryClustersServiceTaskUpdateClusters` gets clustered and unclustered
// visits straddling the threshold and clusters them together. It continues
// doing so, moving the threshold forward 1 day each time, until reaching today.
// When re-clustering clustered visits, it takes all visits in their clusters
// and replaces those clusters. This allows existing clusters to grow without
// having to cluster an impractical number of visits simultaneously and without
// creating near-duplicate clusters. The similar
// `HistoryClustersServiceTaskGetMostRecentClusters` will consume the clusters
// this creates. In contrast to this,
// `HistoryClustersServiceTaskGetMostRecentClusters` iterates recent visits 1st
// and does not persist them.
class HistoryClustersServiceTaskUpdateClusters
    : public HistoryClustersServiceTask {
 public:
  HistoryClustersServiceTaskUpdateClusters(
      base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
      const IncompleteVisitMap incomplete_visit_context_annotations,
      ClusteringBackend* const backend,
      history::HistoryService* const history_service,
      base::OnceClosure callback);
  ~HistoryClustersServiceTaskUpdateClusters() override;

 private:
  // When there remain unclustered visits, cluster them (possibly in combination
  // with clustered visits) and persist the newly created clusters:
  //   Start() ->
  //   GetAnnotatedVisitsToCluster() ->
  //   OnGotModelClusters() ->
  //   OnPersistedClusters() ->
  //   Start()

  // Invoked during construction and after `OnPersistedClusters()` asyncly
  // replaces clusters. Will asyncly request annotated visits from
  // `GetAnnotatedVisitsToCluster()`. May instead syncly invoke `callback_` if
  // there's no `ClusteringBackend` or all visits are exhausted.
  void Start();

  // Invoked after `Start()` asyncly fetches annotated visits. Will asyncly
  // request clusters from `ClusteringBackend`. May instead syncly invoke
  // `callback_` if no annotated visits were fetched.
  void OnGotAnnotatedVisitsToCluster(
      std::vector<int64_t> old_clusters,
      std::vector<history::AnnotatedVisit> annotated_visits,
      QueryClustersContinuationParams continuation_params);

  // Invoked after `OnGotAnnotatedVisitsToCluster()` asyncly obtains clusters.
  // Will asyncly request `old_cluster_ids` be replaced with `clusters`.
  void OnGotModelClusters(std::vector<int64_t> old_cluster_ids,
                          QueryClustersContinuationParams continuation_params,
                          std::vector<history::Cluster> clusters);

  // Invoked after `OnGotModelClusters()` asyncly persists clusters. Will syncly
  // invoke `Start()` to initiate the next iteration.
  void OnPersistedClusters();

  // Never nullptr.
  base::WeakPtr<HistoryClustersService> weak_history_clusters_service_;
  const IncompleteVisitMap incomplete_visit_context_annotations_;
  // Non-owning pointer, but never nullptr.
  ClusteringBackend* const backend_;
  // Non-owning pointer, but never nullptr.
  history::HistoryService* const history_service_;

  // Used to make requests to `GetAnnotatedVisitsToCluster` and
  // `HistoryService`.
  QueryClustersContinuationParams continuation_params_;
  base::CancelableTaskTracker task_tracker_;

  // Invoked after either `Start()` or `OnGotAnnotatedVisitsToCluster()`.
  base::OnceClosure callback_;

  // When `Start()` kicked off the request to fetch visits to cluster.
  base::TimeTicks get_annotated_visits_to_cluster_start_time_;
  // When `OnGotAnnotatedVisitsToCluster()` kicked off the request to cluster
  // the visits.
  base::TimeTicks get_model_clusters_start_time_;
  // When `OnGotModelClusters()` kicked off the request to persist the clusters.
  base::TimeTicks persist_clusters_start_time_;

  // Used for async callbacks.
  base::WeakPtrFactory<HistoryClustersServiceTaskUpdateClusters>
      weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_UPDATE_CLUSTERS_H_
