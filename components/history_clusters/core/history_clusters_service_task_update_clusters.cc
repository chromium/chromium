// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_service_task_update_clusters.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

HistoryClustersServiceTaskUpdateClusters::
    HistoryClustersServiceTaskUpdateClusters(
        base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
        const IncompleteVisitMap incomplete_visit_context_annotations,
        ClusteringBackend* const backend,
        history::HistoryService* const history_service,
        base::OnceClosure callback)
    : weak_history_clusters_service_(std::move(weak_history_clusters_service)),
      incomplete_visit_context_annotations_(
          incomplete_visit_context_annotations),
      backend_(backend),
      history_service_(history_service),
      callback_(std::move(callback)) {
  DCHECK(weak_history_clusters_service_);
  DCHECK(history_service_);
  DCHECK(backend_);
  Start();
}

HistoryClustersServiceTaskUpdateClusters::
    ~HistoryClustersServiceTaskUpdateClusters() = default;

void HistoryClustersServiceTaskUpdateClusters::Start() {
  if (weak_history_clusters_service_ &&
      weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTERS TASK - START. "
        "exhausted_all_visits = %d. "
        "continuation_time = %s.",
        continuation_params_.exhausted_all_visits,
        GetDebugTime(continuation_params_.continuation_time).c_str()));
  }

  if (continuation_params_.exhausted_all_visits) {
    done_ = true;
    std::move(callback_).Run();
    return;
  }

  get_annotated_visits_to_cluster_start_time_ = base::TimeTicks::Now();
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<GetAnnotatedVisitsToCluster>(
          incomplete_visit_context_annotations_, base::Time(),
          continuation_params_, false,
          GetConfig().persist_clusters_recluster_window_days, false,
          base::BindOnce(&HistoryClustersServiceTaskUpdateClusters::
                             OnGotAnnotatedVisitsToCluster,
                         weak_ptr_factory_.GetWeakPtr())),
      &task_tracker_);
}

void HistoryClustersServiceTaskUpdateClusters::OnGotAnnotatedVisitsToCluster(
    std::vector<int64_t> old_clusters,
    std::vector<history::AnnotatedVisit> annotated_visits,
    QueryClustersContinuationParams continuation_params) {
  if (!weak_history_clusters_service_)
    return;

  const auto elapsed_time =
      base::TimeTicks::Now() - get_annotated_visits_to_cluster_start_time_;
  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusters."
      "GetAnnotatedVisitsToClusterLatency",
      elapsed_time);
  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.UpdateClusters.Counts.NumVisitsToCluster",
      static_cast<int>(annotated_visits.size()));

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTERS TASK - VISITS %zu:", annotated_visits.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForVisits(annotated_visits));
  }

  if (annotated_visits.empty()) {
    DCHECK(continuation_params.exhausted_all_visits);
    done_ = true;
    std::move(callback_).Run();
    return;
  }
  get_model_clusters_start_time_ = base::TimeTicks::Now();
  // Using `kAllKeywordCacheRefresh` as that only determines the task priority.
  backend_->GetClusters(
      ClusteringRequestSource::kAllKeywordCacheRefresh,
      base::BindOnce(
          &HistoryClustersServiceTaskUpdateClusters::OnGotModelClusters,
          weak_ptr_factory_.GetWeakPtr(), old_clusters, continuation_params),
      annotated_visits, /*requires_ui_and_triggerability=*/true);
}

void HistoryClustersServiceTaskUpdateClusters::OnGotModelClusters(
    std::vector<int64_t> old_cluster_ids,
    QueryClustersContinuationParams continuation_params,
    std::vector<history::Cluster> clusters) {
  if (!weak_history_clusters_service_)
    return;

  const auto elapsed_time =
      base::TimeTicks::Now() - get_model_clusters_start_time_;
  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusters.ComputeClustersLatency",
      elapsed_time);
  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.UpdateClusters.Counts.NumClustersReplaced",
      static_cast<int>(old_cluster_ids.size()));
  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.UpdateClusters.Counts.NumClustersReturned",
      static_cast<int>(clusters.size()));

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTERS TASK - CLUSTERS %zu:", clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }
  persist_clusters_start_time_ = base::TimeTicks::Now();
  continuation_params_ = continuation_params;
  history_service_->ReplaceClusters(
      old_cluster_ids, clusters,
      base::BindOnce(
          &HistoryClustersServiceTaskUpdateClusters::OnPersistedClusters,
          weak_ptr_factory_.GetWeakPtr()),
      &task_tracker_);
}

void HistoryClustersServiceTaskUpdateClusters::OnPersistedClusters() {
  const auto elapsed_time =
      base::TimeTicks::Now() - persist_clusters_start_time_;
  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusters.PersistClustersLatency",
      elapsed_time);
  Start();
}

}  // namespace history_clusters
