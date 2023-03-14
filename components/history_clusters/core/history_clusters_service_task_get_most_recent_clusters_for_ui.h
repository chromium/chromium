// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_FOR_UI_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_FOR_UI_H_

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
}

namespace history_clusters {

class HistoryClustersService;

// `HistoryClustersServiceTaskGetMostRecentClustersForUI` gets persisted "basic"
// clusters, asks the backend to create UI-ready clusters for them, and invokes
// `callback`. It is an extension of `HistoryClustersService`; rather than
// pollute the latter's namespace with a bunch of callbacks, this class groups
// those callbacks.
class HistoryClustersServiceTaskGetMostRecentClustersForUI
    : public HistoryClustersServiceTask {
 public:
  HistoryClustersServiceTaskGetMostRecentClustersForUI(
      base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
      ClusteringBackend* const backend,
      history::HistoryService* const history_service,
      ClusteringRequestSource clustering_request_source,
      QueryClustersFilterParams filter_params,
      base::Time begin_time,
      QueryClustersContinuationParams continuation_params,
      QueryClustersCallback callback);
  ~HistoryClustersServiceTaskGetMostRecentClustersForUI() override;

 private:
  //   Start() ->
  //   OnGotMostRecentPersistedClusters() ->
  //   OnGotModelClusters()

  // Invoked during construction. Will asyncly request persisted basic clusters.
  void Start(QueryClustersFilterParams filter_params);

  // Invoked after `Start()` asyncly fetches clusters.
  void OnGotMostRecentPersistedClusters(QueryClustersFilterParams filter_params,
                                        base::TimeTicks start_time,
                                        std::vector<history::Cluster> clusters);

  // Invoked after `OnGotMostRecentPersistedClusters()` asyncly obtains
  // clusters. Will syncly invoke `callback_`.
  void OnGotModelClusters(base::TimeTicks start_time,
                          QueryClustersContinuationParams continuation_params,
                          std::vector<history::Cluster> clusters);

  // Never nullptr.
  base::WeakPtr<HistoryClustersService> weak_history_clusters_service_;
  // Non-owning pointer, but never nullptr.
  const raw_ptr<ClusteringBackend> backend_;
  // Non-owning pointer, but never nullptr.
  const raw_ptr<history::HistoryService> history_service_;

  ClusteringRequestSource clustering_request_source_;

  // Used to make requests to `HistoryService`.
  base::Time begin_time_;
  QueryClustersContinuationParams continuation_params_;
  base::CancelableTaskTracker task_tracker_;

  // Invoked after `OnGotModelClusters()`.
  QueryClustersCallback callback_;

  // Used for async callbacks.
  base::WeakPtrFactory<HistoryClustersServiceTaskGetMostRecentClustersForUI>
      weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_FOR_UI_H_
