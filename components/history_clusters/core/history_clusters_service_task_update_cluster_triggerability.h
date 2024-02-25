// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_UPDATE_CLUSTER_TRIGGERABILITY_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_UPDATE_CLUSTER_TRIGGERABILITY_H_

#include <vector>

#include "base/memory/raw_ptr.h"
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

// `HistoryClustersServiceTaskUpdateClusterTriggerability` gets the most recent
// persisted clusters and updates their triggering metadata. It continues doing
// so, moving the threshold forward 1 day each time, until reaching today.
class HistoryClustersServiceTaskUpdateClusterTriggerability
    : public HistoryClustersServiceTask {
 public:
  HistoryClustersServiceTaskUpdateClusterTriggerability(
      base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
      ClusteringBackend* const backend,
      history::HistoryService* const history_service,
      bool likely_has_unclustered_visits_or_unprocessed_clusters,
      base::OnceClosure callback);
  ~HistoryClustersServiceTaskUpdateClusterTriggerability() override;

 private:
  // When there remain unclustered visits, calculate the context clusters and
  // persist them. After unclustered visits have been exhausted, the below flows
  // will be run to calculate triggerability on the remaining clusters.
  //   Start() ->
  //   OnGotAnnotatedVisitsToCluster() ->
  //   OnGotModelContextClusters() ->
  //   OnPersistedContextClusters() ->
  //   Start()

  // When there remain clusters without their triggerability calculated,
  // calculate them and persist the new values:
  //   Start() ->
  //   OnGotPersistedClusters() ->
  //   OnGotModelClustersWithTriggerability() ->
  //   OnPersistedClusterTriggerability() ->
  //   Start()

  // When neither clusters were fetched nor was history exhausted:
  //   Start() ->
  //   OnGotPersistedClusters() ->
  //   Start()

  // Invoked syncly during construction, after `OnGotPersistedClusters()`
  // receives no clusters while history isn't exhuasted, and after
  // `OnPersistedClusterTriggerability()` records metrics. This fetches
  // persisted clusters.
  void Start();

  // Invoked after `Start()` asyncly fetches visits. If visits are returned,
  // this will cluster them into the basic context clusters and persist those.
  // After all unclustered visits have been exhausted, the flow to update its
  // triggerability metadata will be run.
  void OnGotAnnotatedVisitsToCluster(
      base::TimeTicks start_time,
      std::vector<int64_t> old_clusters_unused,
      std::vector<history::AnnotatedVisit> annotated_visits,
      QueryClustersContinuationParams continuation_params);

  // Invoked after `OnGotAnnotatedVisitsToCluster()` asyncly calculates context
  // clusters from unclustered visits.
  void OnGotModelContextClusters(base::TimeTicks start_time,
                                 std::vector<history::Cluster> clusters);

  // Invoked after `OnGotModelContextClusters()` asyncly persists the context
  // clusters.
  void OnPersistedContextClusters(base::TimeTicks start_time);

  // Invoked after `Start()` asyncly fetches clusters when all unclustered
  // visits have been exhausted. May syncly invoke `callback_` if no clusters
  // were returned. If clusters are returned, this will filter for clusters that
  // do not have their triggerability calculated yet so that triggerability
  // metadata can be calculated. Otherwise, it invokes `Start()` to fetch more
  // clusters.
  void OnGotPersistedClusters(base::TimeTicks start_time,
                              std::vector<history::Cluster> clusters);

  // Invoked after `OnGotPersistedClusters()` asyncly obtains clusters.
  void OnGotModelClustersWithTriggerability(
      base::TimeTicks start_time,
      std::vector<history::Cluster> clusters);

  // Invoked after `OnGotModelClustersWithTriggerability()` asyncly persists
  // clusters. Will syncly invoke `Start()` to initiate the next iteration.
  void OnPersistedClusterTriggerability(base::TimeTicks start_time);

  // Marks the task as done, runs `callback_`, and logs metrics about the task
  // run.
  void MarkDoneAndRunCallback();

  // Never nullptr.
  base::WeakPtr<HistoryClustersService> weak_history_clusters_service_;
  // Non-owning pointer, but never nullptr.
  const raw_ptr<ClusteringBackend> backend_;
  // Non-owning pointer, but never nullptr.
  const raw_ptr<history::HistoryService> history_service_;

  // Used to make requests to `HistoryService`.
  QueryClustersContinuationParams continuation_params_;
  base::CancelableTaskTracker task_tracker_;

  // Invoked after `OnGotPersistedClusters()` when all clusters have been
  // exhausted.
  base::OnceClosure callback_;

  // Tracks the time `this` was created to use for the max time we should update
  // clusters for.
  base::Time task_created_time_;

  // Whether it is likely that there are unclustered visits to cluster or old
  // clusters that do not yet have their triggerability calculated.
  const bool likely_has_unclustered_visits_or_unprocessed_clusters_ = false;

  // Tracks whether at least one cluster's triggerability was updated (for
  // metrics only).
  bool updated_cluster_triggerability_ = false;

  // Tracks whether at least one cluster's triggerability was updated after the
  // first call with all filtered clusters returned (for metrics only).
  // Initially nullopt but false after receiving a response with no clusters and
  // true after receiving a response with clusters after having received a
  // response with no clusters.
  std::optional<bool>
      updated_cluster_triggerability_after_filtered_clusters_empty_;

  // Used for async callbacks.
  base::WeakPtrFactory<HistoryClustersServiceTaskUpdateClusterTriggerability>
      weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_UPDATE_CLUSTER_TRIGGERABILITY_H_
