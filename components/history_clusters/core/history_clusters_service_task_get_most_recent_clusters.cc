// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_service_task_get_most_recent_clusters.h"

#include <utility>

#include "base/bind.h"
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
        QueryClustersCallback callback)
    : weak_history_clusters_service_(std::move(weak_history_clusters_service)),
      incomplete_visit_context_annotations_(
          incomplete_visit_context_annotations),
      backend_(backend),
      history_service_(history_service),
      clustering_request_source_(clustering_request_source),
      begin_time_(begin_time),
      continuation_params_(continuation_params),
      callback_(std::move(callback)) {
  DCHECK(weak_history_clusters_service_);
  DCHECK(history_service_);
  Start();
}

HistoryClustersServiceTaskGetMostRecentClusters::
    ~HistoryClustersServiceTaskGetMostRecentClusters() = default;

void HistoryClustersServiceTaskGetMostRecentClusters::Start() {
  // Shouldn't request more clusters if history has been exhausted.
  DCHECK(!continuation_params_.is_done);

  if (!backend_) {
    // Early exit if we won't be able to cluster visits.
    weak_history_clusters_service_->NotifyDebugMessage(
        "HistoryClustersService::QueryClusters Error: ClusteringBackend is "
        "nullptr. Returning empty cluster vector.");
    done_ = true;
    std::move(callback_).Run({}, QueryClustersContinuationParams::DoneParams());

  } else {
    history_service_get_annotated_visits_to_cluster_start_time_ =
        base::TimeTicks::Now();
    history_service_->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetAnnotatedVisitsToCluster>(
            incomplete_visit_context_annotations_, begin_time_,
            continuation_params_,
            base::BindOnce(&HistoryClustersServiceTaskGetMostRecentClusters::
                               OnGotAnnotatedVisitsToCluster,
                           weak_ptr_factory_.GetWeakPtr())),
        &task_tracker_);
  }
}

void HistoryClustersServiceTaskGetMostRecentClusters::
    OnGotAnnotatedVisitsToCluster(
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
      "Histogram.Clusters.Backend.QueryAnnotatedVisitsLatency",
      base::TimeTicks::Now() -
          history_service_get_annotated_visits_to_cluster_start_time_);

  if (annotated_visits.empty()) {
    // Early exit without calling backend if there's no annotated visits.
    done_ = true;
    std::move(callback_).Run({}, QueryClustersContinuationParams::DoneParams());

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

}  // namespace history_clusters
