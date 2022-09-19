// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_page_handler_impl.h"

#include "base/time/time.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/history_clusters_debug_jsons.h"

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

void HistoryClustersInternalsPageHandlerImpl::GetVisitsJson(
    GetVisitsJsonCallback callback) {
  if (!history_service_) {
    std::move(callback).Run("");
    return;
  }
  GetAnnotatedVisits(history_clusters::QueryClustersContinuationParams(),
                     /*previously_retrieved_visits=*/{}, std::move(callback));
}

void HistoryClustersInternalsPageHandlerImpl::
    PrintKeywordBagStateToLogMessages() {
  if (history_clusters_service_) {
    history_clusters_service_->PrintKeywordBagStateToLogMessage();
  } else {
    OnDebugMessage("Service is nullptr.");
  }
}

void HistoryClustersInternalsPageHandlerImpl::GetAnnotatedVisits(
    history_clusters::QueryClustersContinuationParams continuation_params,
    std::vector<history::AnnotatedVisit> previously_retrieved_visits,
    GetVisitsJsonCallback callback) {
  // There are two forms of cancellation here because `ScheduleDBTask` does
  // not take in a callback.
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<history_clusters::GetAnnotatedVisitsToCluster>(
          history_clusters::IncompleteVisitMap(), /*begin_time=*/base::Time(),
          continuation_params,
          /*recent_first=*/true,
          /*days_of_clustered_visits=*/0, /*recluster=*/true,
          base::BindOnce(
              &HistoryClustersInternalsPageHandlerImpl::OnGotAnnotatedVisits,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(previously_retrieved_visits), std::move(callback))),
      &task_tracker_);
}

void HistoryClustersInternalsPageHandlerImpl::OnGotAnnotatedVisits(
    std::vector<history::AnnotatedVisit> previously_retrieved_visits,
    GetVisitsJsonCallback callback,
    std::vector<int64_t> old_clusters,
    std::vector<history::AnnotatedVisit> annotated_visits,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  previously_retrieved_visits.insert(previously_retrieved_visits.end(),
                                     annotated_visits.begin(),
                                     annotated_visits.end());
  if (continuation_params.exhausted_all_visits) {
    std::move(callback).Run(
        history_clusters::GetDebugJSONForVisits(previously_retrieved_visits));
    return;
  }

  GetAnnotatedVisits(continuation_params,
                     std::move(previously_retrieved_visits),
                     std::move(callback));
}

void HistoryClustersInternalsPageHandlerImpl::OnDebugMessage(
    const std::string& message) {
  page_->OnLogMessageAdded(message);
}
