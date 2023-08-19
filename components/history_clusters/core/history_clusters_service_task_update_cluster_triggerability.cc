// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service_task_update_cluster_triggerability.h"

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

HistoryClustersServiceTaskUpdateClusterTriggerability::
    HistoryClustersServiceTaskUpdateClusterTriggerability(
        base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
        ClusteringBackend* const backend,
        history::HistoryService* const history_service,
        bool likely_has_unclustered_visits_or_unprocessed_clusters,
        base::OnceClosure callback)
    : weak_history_clusters_service_(std::move(weak_history_clusters_service)),
      backend_(backend),
      history_service_(history_service),
      callback_(std::move(callback)),
      task_created_time_(base::Time::Now()),
      likely_has_unclustered_visits_or_unprocessed_clusters_(
          likely_has_unclustered_visits_or_unprocessed_clusters) {
  DCHECK(weak_history_clusters_service_);
  DCHECK(history_service_);
  DCHECK(backend_);

  continuation_params_.continuation_time = task_created_time_;
  continuation_params_.exhausted_unclustered_visits =
      !likely_has_unclustered_visits_or_unprocessed_clusters_;
  Start();
}

HistoryClustersServiceTaskUpdateClusterTriggerability::
    ~HistoryClustersServiceTaskUpdateClusterTriggerability() = default;

void HistoryClustersServiceTaskUpdateClusterTriggerability::Start() {
  if (weak_history_clusters_service_ &&
      weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTER TRIGGERABILITY TASK - START. "
        "exhausted_unclustered_visits = %d. "
        "continuation_time = %s.",
        continuation_params_.exhausted_unclustered_visits,
        GetDebugTime(continuation_params_.continuation_time).c_str()));
  }

  if (continuation_params_.exhausted_unclustered_visits) {
    history_service_->GetMostRecentClusters(
        base::Time::Min(), continuation_params_.continuation_time,
        GetConfig().max_persisted_clusters_to_fetch,
        GetConfig().max_persisted_cluster_visits_to_fetch_soft_cap,
        base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                           OnGotPersistedClusters,
                       weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
        /*include_keywords_and_duplicates=*/false, &task_tracker_);
  } else {
    history_service_->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetAnnotatedVisitsToCluster>(
            std::map<int64_t, IncompleteVisitContextAnnotations>(),
            /*begin_time=*/base::Time::Min(), continuation_params_,
            /*recent_first=*/true,
            /*days_of_clustered_visits=*/0, /*recluster=*/false,
            base::BindOnce(
                &HistoryClustersServiceTaskUpdateClusterTriggerability::
                    OnGotAnnotatedVisitsToCluster,
                weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now())),
        &task_tracker_);
  }
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    OnGotAnnotatedVisitsToCluster(
        base::TimeTicks start_time,
        std::vector<int64_t> old_clusters_unused,
        std::vector<history::AnnotatedVisit> annotated_visits,
        QueryClustersContinuationParams continuation_params) {
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "GetAnnotatedVisitsToClusterLatency",
      base::TimeTicks::Now() - start_time);

  // Records whether there were unclustered visits to cluster. This will inform
  // whether the call to get annotated visits to cluster can be removed once
  // this code path has launched.
  if (!continuation_params_.is_continuation) {
    base::UmaHistogramBoolean(
        "History.Clusters.Backend.UpdateClusterTriggerability."
        "HadUnclusteredVisitsToCluster",
        annotated_visits.empty());
  }

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTER TRIGGERABILITY TASK - UNCLUSTERED VISITS %zu:",
        annotated_visits.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForVisits(annotated_visits));
  }

  continuation_params_ = continuation_params;

  if (annotated_visits.empty()) {
    // Reset the continuation time so that the newly created clusters have their
    // triggerability metadata calculated.
    continuation_params_.continuation_time = task_created_time_;
    Start();
    return;
  }

  // Using `kAllKeywordCacheRefresh` as that determines the task priority.
  backend_->GetClusters(
      ClusteringRequestSource::kAllKeywordCacheRefresh,
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                         OnGotModelContextClusters,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      annotated_visits, /*requires_ui_and_triggerability=*/false);
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    OnGotModelContextClusters(base::TimeTicks start_time,
                              std::vector<history::Cluster> clusters) {
  // Persist the context clusters.
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "ComputeContextClustersLatency",
      base::TimeTicks::Now() - start_time);

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTER TRIGGERABILITY TASK - MODEL CONTEXT CLUSTERS %zu:",
        clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }

  history_service_->ReplaceClusters(
      /*ids_to_delete=*/{}, clusters,
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                         OnPersistedContextClusters,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      &task_tracker_);
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    OnPersistedContextClusters(base::TimeTicks start_time) {
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "PersistContextClustersLatency",
      base::TimeTicks::Now() - start_time);

  Start();
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    OnGotPersistedClusters(base::TimeTicks start_time,
                           std::vector<history::Cluster> clusters) {
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "GetMostRecentPersistedClustersLatency",
      base::TimeTicks::Now() - start_time);

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTER TRIGGERABILITY TASK - PERSISTED CONTEXT CLUSTERS %zu:",
        clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }

  if (clusters.empty()) {
    // TODO(manukh): If the most recent cluster is invalid (due to DB
    //  corruption), `GetMostRecentClusters()` will return no clusters. We
    //  should handle this case and not assume we've exhausted history.
    MarkDoneAndRunCallback();
    return;
  }
  continuation_params_.continuation_time =
      clusters.back().GetMostRecentVisit().annotated_visit.visit_row.visit_time;

  std::vector<history::Cluster> filtered_clusters;
  base::ranges::copy_if(std::make_move_iterator(clusters.begin()),
                        std::make_move_iterator(clusters.end()),
                        std::back_inserter(filtered_clusters),
                        [&](const history::Cluster& cluster) {
                          return !cluster.triggerability_calculated;
                        });

  if (filtered_clusters.empty()) {
    if (!updated_cluster_triggerability_after_filtered_clusters_empty_) {
      // If nullopt, set initial state to false.
      updated_cluster_triggerability_after_filtered_clusters_empty_ = false;
    }

    if (likely_has_unclustered_visits_or_unprocessed_clusters_) {
      // Get more persisted clusters and see if the clusters need to be updated
      // if visits have not been exhausted yet.
      Start();
    } else {
      MarkDoneAndRunCallback();
    }
    return;
  }

  updated_cluster_triggerability_ = true;

  if (updated_cluster_triggerability_after_filtered_clusters_empty_) {
    // If this is a run after all returned clusters had their cluster
    // triggerability calculated, set this value to true since there were
    // clusters that did not have their triggerability calculated earlier in the
    // user's history.
    updated_cluster_triggerability_after_filtered_clusters_empty_ = true;
  }

  backend_->GetClusterTriggerability(
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                         OnGotModelClustersWithTriggerability,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      std::move(filtered_clusters));
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    OnGotModelClustersWithTriggerability(
        base::TimeTicks start_time,
        std::vector<history::Cluster> clusters) {
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "ComputeClusterTriggerabilityLatency",
      base::TimeTicks::Now() - start_time);

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(
        base::StringPrintf("UPDATE CLUSTER TRIGGERABILITY TASK - MODEL "
                           "CLUSTERS WITH TRIGGERABILITY %zu:",
                           clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }

  // Override the triggerability persistence if the most recent visit is within
  // the cutoff period.
  for (auto& cluster : clusters) {
    base::Time cluster_most_recent_visit_time =
        cluster.GetMostRecentVisit().annotated_visit.visit_row.visit_time;
    if (task_created_time_ - cluster_most_recent_visit_time <
        GetConfig().cluster_triggerability_cutoff_duration) {
      cluster.triggerability_calculated = false;
    }
  }

  history_service_->UpdateClusterTriggerability(
      clusters,
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                         OnPersistedClusterTriggerability,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      &task_tracker_);
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    OnPersistedClusterTriggerability(base::TimeTicks start_time) {
  base::UmaHistogramTimes(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "PersistClusterTriggerabilityLatency",
      base::TimeTicks::Now() - start_time);

  Start();
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::
    MarkDoneAndRunCallback() {
  done_ = true;

  base::UmaHistogramBoolean(
      "History.Clusters.Backend.UpdateClusterTriggerability."
      "DidUpdateClusterTriggerability",
      updated_cluster_triggerability_);
  if (updated_cluster_triggerability_after_filtered_clusters_empty_) {
    base::UmaHistogramBoolean(
        "History.Clusters.Backend.UpdateClusterTriggerability."
        "DidUpdateClusterTriggerability.AfterFilteredClustersEmpty",
        *updated_cluster_triggerability_after_filtered_clusters_empty_);
  }

  std::move(callback_).Run();
}

}  // namespace history_clusters
