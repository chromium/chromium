// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_page_handler_impl.h"

#include "base/time/time.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_service_task_get_most_recent_clusters.h"
#include "components/history_clusters/core/history_clusters_util.h"

HistoryClustersInternalsPageHandlerImpl::
    HistoryClustersInternalsPageHandlerImpl(
        mojo::PendingRemote<history_clusters_internals::mojom::Page> page,
        mojo::PendingReceiver<history_clusters_internals::mojom::PageHandler>
            pending_page_handler,
        history_clusters::HistoryClustersService* history_clusters_service,
        history::HistoryService* history_service)
    : page_(std::move(page)),
      page_handler_(this, std::move(pending_page_handler)),
      history_clusters_service_(history_clusters_service),
      history_service_(history_service) {
  if (!history_clusters::GetConfig().history_clusters_internals_page) {
    page_->OnLogMessageAdded(
        "History clusters internals page feature is turned off.");
    return;
  }
  if (!history_clusters_service_) {
    page_->OnLogMessageAdded(
        "History clusters service not found for the profile.");
    return;
  }
  history_clusters_service_->AddObserver(this);
}

HistoryClustersInternalsPageHandlerImpl::
    ~HistoryClustersInternalsPageHandlerImpl() {
  if (history_clusters_service_)
    history_clusters_service_->RemoveObserver(this);
}

void HistoryClustersInternalsPageHandlerImpl::GetContextClustersJson(
    GetContextClustersJsonCallback callback) {
  if (history_clusters_service_ &&
      history_clusters::ShouldUseNavigationContextClustersFromPersistence()) {
    GetContextClusters(
        history_clusters::QueryClustersContinuationParams{
            /*continuation_time=*/base::Time::Now(), /*is_continuation=*/true,
            /*is_partial_day=*/false, /*exhausted_unclustered_visits=*/true,
            /*exhausted_all_visits=*/false},
        /*previously_retrieved_clusters=*/{}, std::move(callback));
  } else {
    std::move(callback).Run("");
    return;
  }
}

void HistoryClustersInternalsPageHandlerImpl::GetContextClusters(
    history_clusters::QueryClustersContinuationParams continuation_params,
    std::vector<history::Cluster> previously_retrieved_clusters,
    GetContextClustersJsonCallback callback) {
  // Querying context clusters for a non-UI source, as internals page would be
  // used sparingly using any non-UI source should be fine.
  query_context_clusters_task_ = history_clusters_service_->QueryClusters(
      history_clusters::ClusteringRequestSource::kAllKeywordCacheRefresh,
      history_clusters::QueryClustersFilterParams(),
      /*begin_time=*/base::Time(), continuation_params, /*recluster=*/false,
      base::BindOnce(
          &HistoryClustersInternalsPageHandlerImpl::OnGotContextClusters,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(previously_retrieved_clusters), std::move(callback)));
}

void HistoryClustersInternalsPageHandlerImpl::OnGotContextClusters(
    std::vector<history::Cluster> previously_retrieved_clusters,
    GetContextClustersJsonCallback callback,
    std::vector<history::Cluster> new_clusters,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  previously_retrieved_clusters.insert(previously_retrieved_clusters.end(),
                                       new_clusters.begin(),
                                       new_clusters.end());
  if (continuation_params.exhausted_all_visits) {
    std::move(callback).Run(history_clusters::GetDebugJSONForClusters(
        previously_retrieved_clusters));
    return;
  }
  GetContextClusters(continuation_params,
                     /*previously_retrieved_clusters=*/
                     std::move(previously_retrieved_clusters),
                     /*callback=*/std::move(callback));
}

void HistoryClustersInternalsPageHandlerImpl::
    PrintKeywordBagStateToLogMessages() {
  if (history_clusters_service_) {
    history_clusters_service_->PrintKeywordBagStateToLogMessage();
  } else {
    OnDebugMessage("Service is nullptr.");
  }
}

void HistoryClustersInternalsPageHandlerImpl::OnDebugMessage(
    const std::string& message) {
  page_->OnLogMessageAdded(message);
}
