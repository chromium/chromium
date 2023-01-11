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
#include "components/history_clusters/core/history_clusters_debug_jsons.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

HistoryClustersServiceTaskUpdateClusterTriggerability::
    HistoryClustersServiceTaskUpdateClusterTriggerability(
        base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
        ClusteringBackend* const backend,
        history::HistoryService* const history_service,
        base::OnceClosure callback)
    : weak_history_clusters_service_(std::move(weak_history_clusters_service)),
      backend_(backend),
      history_service_(history_service),
      callback_(std::move(callback)),
      task_created_time_(base::Time::Now()) {
  DCHECK(weak_history_clusters_service_);
  DCHECK(history_service_);
  DCHECK(backend_);

  continuation_time_ = task_created_time_;
  Start();
}

HistoryClustersServiceTaskUpdateClusterTriggerability::
    ~HistoryClustersServiceTaskUpdateClusterTriggerability() = default;

void HistoryClustersServiceTaskUpdateClusterTriggerability::Start() {
  history_service_->GetMostRecentClusters(
      base::Time::Min(), continuation_time_,
      GetConfig().max_persisted_clusters_to_fetch,
      GetConfig().max_persisted_cluster_visits_to_fetch_soft_cap,
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                         OnGotPersistedClusters,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      /*include_keywords_and_duplicates=*/false, &task_tracker_);
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
        "UPDATE CLUSTER TRIGGERABILITY TASK - PERSISTED CLUSTERS %zu:",
        clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }

  if (clusters.empty()) {
    // TODO(manukh): If the most recent cluster is invalid (due to DB
    //  corruption), `GetMostRecentClusters()` will return no clusters. We
    //  should handle this case and not assume we've exhausted history.
    done_ = true;
    std::move(callback_).Run();
    return;
  }
  continuation_time_ =
      clusters.back().GetMostRecentVisit().annotated_visit.visit_row.visit_time;

  std::vector<history::Cluster> filtered_clusters;
  base::ranges::copy_if(clusters, std::back_inserter(filtered_clusters),
                        [&](const history::Cluster& cluster) {
                          return !cluster.triggerability_calculated;
                        });

  if (filtered_clusters.empty()) {
    // Get more persisted clusters and see if the clusters need to be updated if
    // visits have not been exhausted yet.
    Start();
    return;
  }

  backend_->GetClusterTriggerability(
      base::BindOnce(&HistoryClustersServiceTaskUpdateClusterTriggerability::
                         OnGotModelClusters,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      std::move(filtered_clusters));
}

void HistoryClustersServiceTaskUpdateClusterTriggerability::OnGotModelClusters(
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
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "UPDATE CLUSTER TRIGGERABILITY TASK - MODEL CLUSTERS %zu:",
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

}  // namespace history_clusters
