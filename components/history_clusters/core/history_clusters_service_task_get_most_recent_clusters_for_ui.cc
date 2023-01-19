// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_service_task_get_most_recent_clusters_for_ui.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

HistoryClustersServiceTaskGetMostRecentClustersForUI::
    HistoryClustersServiceTaskGetMostRecentClustersForUI(
        base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
        ClusteringBackend* const backend,
        history::HistoryService* const history_service,
        base::Time begin_time,
        QueryClustersContinuationParams continuation_params,
        QueryClustersCallback callback)
    : weak_history_clusters_service_(std::move(weak_history_clusters_service)),
      backend_(backend),
      history_service_(history_service),
      begin_time_(begin_time),
      continuation_params_(continuation_params),
      callback_(std::move(callback)) {
  DCHECK(weak_history_clusters_service_);
  DCHECK(history_service_);
  DCHECK(backend_);

  Start();
}

HistoryClustersServiceTaskGetMostRecentClustersForUI::
    ~HistoryClustersServiceTaskGetMostRecentClustersForUI() = default;

void HistoryClustersServiceTaskGetMostRecentClustersForUI::Start() {
  // TODO(b/259466296): Figure out what to do with unclustered visits that
  //   happen before this experiment starts and were unclustered by previous
  //   path.

  if (!continuation_params_.is_continuation) {
    continuation_params_.continuation_time = base::Time::Now();
  }

  history_service_->GetMostRecentClusters(
      begin_time_, continuation_params_.continuation_time,
      GetConfig().max_persisted_clusters_to_fetch,
      GetConfig().max_persisted_cluster_visits_to_fetch_soft_cap,
      base::BindOnce(&HistoryClustersServiceTaskGetMostRecentClustersForUI::
                         OnGotMostRecentPersistedClusters,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      /*include_keywords_and_duplicates=*/false, &task_tracker_);
}

void HistoryClustersServiceTaskGetMostRecentClustersForUI::
    OnGotMostRecentPersistedClusters(base::TimeTicks start_time,
                                     std::vector<history::Cluster> clusters) {
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.GetMostRecentClustersForUI."
      "GetMostRecentPersistedClustersLatency",
      base::TimeTicks::Now() - start_time);

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(
        base::StringPrintf("GET MOST RECENT CLUSTERS FOR UI TASK - PERSISTED "
                           "CONTEXT CLUSTERS %zu:",
                           clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }

  // TODO(manukh): If the most recent cluster is invalid (due to DB corruption),
  //  `GetMostRecentClusters()` will return no clusters. We should handle this
  //  case and not assume we've exhausted history.
  auto continuation_params =
      clusters.empty() ? QueryClustersContinuationParams::DoneParams()
                       : QueryClustersContinuationParams{
                             clusters.back()
                                 .GetMostRecentVisit()
                                 .annotated_visit.visit_row.visit_time,
                             true, false, true, false};

  // Prune out synced clusters if feature not enabled.
  if (!GetConfig().include_synced_visits) {
    auto it = clusters.begin();
    while (it != clusters.end()) {
      if (it->originator_cache_guid.empty()) {
        it++;
      } else {
        it = clusters.erase(it);
      }
    }
  }

  backend_->GetClustersForUI(
      base::BindOnce(&HistoryClustersServiceTaskGetMostRecentClustersForUI::
                         OnGotModelClusters,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(continuation_params)),
      std::move(clusters));
}

void HistoryClustersServiceTaskGetMostRecentClustersForUI::OnGotModelClusters(
    base::TimeTicks start_time,
    QueryClustersContinuationParams continuation_params,
    std::vector<history::Cluster> clusters) {
  if (!weak_history_clusters_service_) {
    return;
  }

  base::UmaHistogramTimes(
      "History.Clusters.Backend.GetMostRecentClustersForUI."
      "ComputeClustersForUILatency",
      base::TimeTicks::Now() - start_time);

  if (weak_history_clusters_service_->ShouldNotifyDebugMessage()) {
    weak_history_clusters_service_->NotifyDebugMessage(base::StringPrintf(
        "GET MOST RECENT CLUSTERS FOR UI TASK - CLUSTERS FOR UI %zu:",
        clusters.size()));
    weak_history_clusters_service_->NotifyDebugMessage(
        GetDebugJSONForClusters(clusters));
  }
  done_ = true;
  std::move(callback_).Run(std::move(clusters), std::move(continuation_params));
}

}  // namespace history_clusters
