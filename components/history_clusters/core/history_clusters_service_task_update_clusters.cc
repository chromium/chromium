// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_service_task_update_clusters.h"

#include <utility>

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"

namespace history_clusters {

HistoryClustersServiceTaskUpdateClusters::
    HistoryClustersServiceTaskUpdateClusters(
        const IncompleteVisitMap incomplete_visit_context_annotations,
        ClusteringBackend* const backend,
        history::HistoryService* const history_service,
        base::OnceClosure callback)
    : incomplete_visit_context_annotations_(
          incomplete_visit_context_annotations),
      backend_(backend),
      history_service_(history_service),
      callback_(std::move(callback)) {
  Start();
}

HistoryClustersServiceTaskUpdateClusters::
    ~HistoryClustersServiceTaskUpdateClusters() = default;

void HistoryClustersServiceTaskUpdateClusters::Start() {
  if (!backend_ || continuation_params_.exhausted_all_visits) {
    done_ = true;
    std::move(callback_).Run();
    return;
  }
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<GetAnnotatedVisitsToCluster>(
          incomplete_visit_context_annotations_, base::Time(),
          continuation_params_, false, 2, false,
          base::BindOnce(&HistoryClustersServiceTaskUpdateClusters::
                             OnGotAnnotatedVisitsToCluster,
                         weak_ptr_factory_.GetWeakPtr())),
      &task_tracker_);
}

void HistoryClustersServiceTaskUpdateClusters::OnGotAnnotatedVisitsToCluster(
    std::vector<int64_t> old_clusters,
    std::vector<history::AnnotatedVisit> annotated_visits,
    QueryClustersContinuationParams continuation_params) {
  if (annotated_visits.empty()) {
    DCHECK(continuation_params.exhausted_all_visits);
    done_ = true;
    std::move(callback_).Run();
    return;
  }
  // Using `kKeywordCacheGeneration` as that only determines the task priority.
  backend_->GetClusters(
      ClusteringRequestSource::kKeywordCacheGeneration,
      base::BindOnce(
          &HistoryClustersServiceTaskUpdateClusters::OnGotModelClusters,
          weak_ptr_factory_.GetWeakPtr(), old_clusters, continuation_params),
      annotated_visits);
}

void HistoryClustersServiceTaskUpdateClusters::OnGotModelClusters(
    std::vector<int64_t> old_cluster_ids,
    QueryClustersContinuationParams continuation_params,
    std::vector<history::Cluster> clusters) {
  continuation_params_ = continuation_params;
  history_service_->ReplaceClusters(
      old_cluster_ids, clusters,
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusters::Start,
                     weak_ptr_factory_.GetWeakPtr()),
      &task_tracker_);
}

}  // namespace history_clusters
