// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_service_task_get_most_recent_clusters.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time_to_iso8601.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"

namespace history_clusters {

HistoryClustersServiceTaskGetMostRecentClusters::
    HistoryClustersServiceTaskGetMostRecentClusters(
        base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
        const IncompleteVisitMap incomplete_visit_context_annotations,
        ClusteringBackend* const backend,
        history::HistoryService* const history_service,
        ClusteringRequestSource clustering_request_source,
        base::Time begin_time,
        QueryClustersContinuationParams continuation_params,
        bool recluster,
        QueryClustersCallback callback)
    : weak_history_clusters_service_(std::move(weak_history_clusters_service)),
      incomplete_visit_context_annotations_(
          incomplete_visit_context_annotations),
      backend_(backend),
      history_service_(history_service),
      clustering_request_source_(clustering_request_source),
      begin_time_(begin_time),
      continuation_params_(continuation_params),
      recluster_(recluster),
      callback_(std::move(callback)) {
  DCHECK(weak_history_clusters_service_);
  DCHECK(history_service_);
  Start();
}

HistoryClustersServiceTaskGetMostRecentClusters::
    ~HistoryClustersServiceTaskGetMostRecentClusters() = default;

void HistoryClustersServiceTaskGetMostRecentClusters::Start() {
  // Shouldn't request more clusters if history has been exhausted.
  DCHECK(!continuation_params_.exhausted_all_visits);

  if (!backend_ || continuation_params_.exhausted_unclustered_visits) {
    // If visits can't be clustered, either because `backend_` is null, or all
    // unclustered visits have already been clustered and returned, then return
    // persisted clusters.
    if (!backend_) {
      weak_history_clusters_service_->NotifyDebugMessage(
          "HistoryClustersServiceTaskGetMostRecentClusters::Start() Error: "
          "ClusteringBackend is nullptr. Returning most recent clusters.");
    } else {
      weak_history_clusters_service_->NotifyDebugMessage(
          "HistoryClustersServiceTaskGetMostRecentClusters::Start() exhausted "
          "unclustered visits. Returning most recent clusters.");
    }
    ReturnMostRecentPersistedClusters(continuation_params_.continuation_time);

  } else {
    // TODO(manukh): It's not clear how to blend unclustered and clustered
    //  visits when iterating recent first. E.g., if we have 4 days of
    //  unclustered visits, should the most recent 3 be clustered in isolation,
    //  while the 4th is clustered with older clustered visits? For now, we do
    //  the simplest approach: cluster each day in isolation. If updating
    //  clusters occurs frequently enough, this issue will be mitigated.
    //  However, since the top, most prominent clusters will be the most recent
    //  clusters, and current-day visits will never be pre-clustered, we
    //  probably want to make sure they're optimal. So we should probably not
    //  cluster at least the current day in isolation.
    history_service_get_annotated_visits_to_cluster_start_time_ =
        base::TimeTicks::Now();
    history_service_->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetAnnotatedVisitsToCluster>(
            incomplete_visit_context_annotations_, begin_time_,
            continuation_params_, true, 0, recluster_,
            base::BindOnce(&HistoryClustersServiceTaskGetMostRecentClusters::
                               OnGotAnnotatedVisitsToCluster,
                           weak_ptr_factory_.GetWeakPtr())),
        &task_tracker_);
  }
}

void HistoryClustersServiceTaskGetMostRecentClusters::
    OnGotAnnotatedVisitsToCluster(
        // Unused because clusters aren't persisted in this flow.
        std::vector<int64_t> old_clusters_unused,
        std::vector<history::AnnotatedVisit> annotated_visits,
        QueryClustersContinuationParams continuation_params) {
  DCHECK(backend_);

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(
        "HistoryClustersServiceTaskGetMostRecentClusters::OnGotHistoryVisits("
        ")");
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "  annotated_visits.size() = %zu", annotated_visits.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        "  continuation_time = " +
        (continuation_params.continuation_time.is_null()
             ? "null (i.e. exhausted history)"
             : base::TimeToISO8601(continuation_params.continuation_time)));
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.QueryAnnotatedVisitsLatency",
      base::TimeTicks::Now() -
          history_service_get_annotated_visits_to_cluster_start_time_);

  if (annotated_visits.empty()) {
    // If there're no unclustered visits to cluster, then return persisted
    // clusters.
    ReturnMostRecentPersistedClusters(continuation_params.continuation_time);

  } else {
    if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
      weak_history_clusters_service_->NotifyDebugMessage(
          "  Visits JSON follows:");
      weak_history_clusters_service_->NotifyDebugMessage(
          GetDebugJSONForVisits(annotated_visits));
      weak_history_clusters_service_->NotifyDebugMessage(
          "Calling backend_->GetClusters()");
    }
    base::UmaHistogramCounts1000("History.Clusters.Backend.NumVisitsToCluster",
                                 static_cast<int>(annotated_visits.size()));
    backend_get_clusters_start_time_ = base::TimeTicks::Now();
    backend_->GetClusters(
        clustering_request_source_,
        base::BindOnce(&HistoryClustersServiceTaskGetMostRecentClusters::
                           OnGotModelClusters,
                       weak_ptr_factory_.GetWeakPtr(), continuation_params),
        std::move(annotated_visits));
  }
}

void HistoryClustersServiceTaskGetMostRecentClusters::OnGotModelClusters(
    QueryClustersContinuationParams continuation_params,
    std::vector<history::Cluster> clusters) {
  base::UmaHistogramTimes(
      "History.Clusters.Backend.GetClustersLatency",
      base::TimeTicks::Now() - backend_get_clusters_start_time_);
  base::UmaHistogramCounts1000("History.Clusters.Backend.NumClustersReturned",
                               clusters.size());

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(
        "HistoryClustersService::OnGotRawClusters()");
    weak_history_clusters_service_->NotifyDebugMessage(
        "  Raw Clusters from Backend JSON follows:");
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }

  done_ = true;
  std::move(callback_).Run(clusters, continuation_params);
}

void HistoryClustersServiceTaskGetMostRecentClusters::
    ReturnMostRecentPersistedClusters(base::Time exclusive_max_time) {
  if (GetConfig().persist_clusters_in_history_db && !recluster_) {
    history_service_->GetMostRecentClusters(
        begin_time_, exclusive_max_time, 1,
        base::BindOnce(&HistoryClustersServiceTaskGetMostRecentClusters::
                           OnGotMostRecentPersistedClusters,
                       weak_ptr_factory_.GetWeakPtr()),
        &task_tracker_);
  } else {
    OnGotMostRecentPersistedClusters({});
  }
}

void HistoryClustersServiceTaskGetMostRecentClusters::
    OnGotMostRecentPersistedClusters(std::vector<history::Cluster> clusters) {
  auto continuation_params =
      clusters.empty() ? QueryClustersContinuationParams::DoneParams()
                       : QueryClustersContinuationParams{
                             clusters[0]
                                 .GetMostRecentVisit()
                                 .annotated_visit.visit_row.visit_time,
                             true, false, true, false};
  done_ = true;
  std::move(callback_).Run(clusters, continuation_params);
}

}  // namespace history_clusters
