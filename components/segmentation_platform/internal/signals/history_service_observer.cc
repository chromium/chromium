// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/history_service_observer.h"

#include "base/metrics/user_metrics.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/signals/history_delegate_impl.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {

HistoryServiceObserver::HistoryServiceObserver(
    history::HistoryService* history_service,
    StorageService* storage_service,
    const std::string& profile_id,
    base::RepeatingClosure models_refresh_callback)
    : storage_service_(storage_service),
      url_signal_handler_(
          storage_service->ukm_data_manager()->GetOrCreateUrlHandler()),
      models_refresh_callback_(models_refresh_callback),
      profile_id_(profile_id),
      history_delegate_(
          std::make_unique<HistoryDelegateImpl>(history_service,
                                                url_signal_handler_,
                                                profile_id)) {
  history_observation_.Observe(history_service);
}
HistoryServiceObserver::HistoryServiceObserver()
    : storage_service_(nullptr), url_signal_handler_(nullptr) {}

HistoryServiceObserver::~HistoryServiceObserver() = default;

void HistoryServiceObserver::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& new_visit) {
  url_signal_handler_->OnHistoryVisit(url_row.url(), profile_id_);
  history_delegate_->OnUrlAdded(url_row.url());
}

void HistoryServiceObserver::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  TRACE_EVENT0("segmentation_platform",
               "HistoryServiceObserver::OnHistoryDeletions");

  // If the history deletion was not from expiration or if the whole history
  // database was removed, delete the segment results computed based on URL
  // data.
  if (deletion_info.IsAllHistory() || !deletion_info.is_from_expiration()) {
    base::RecordAction(
        base::UserMetricsAction("SegmentationPurgeTriggeredByHistoryDelete"));
    DeleteResultsForHistoryBasedSegments();
  }

  if (deletion_info.IsAllHistory()) {
    url_signal_handler_->OnUrlsRemovedFromHistory({}, /*all_urls=*/true);
    return;
  }
  std::vector<GURL> urls;
  for (const auto& info : deletion_info.deleted_rows())
    urls.push_back(info.url());
  url_signal_handler_->OnUrlsRemovedFromHistory(urls, /*all_urls=*/false);
  history_delegate_->OnUrlRemoved(urls);
}

void HistoryServiceObserver::SetHistoryBasedSegments(
    base::flat_set<proto::SegmentId> history_based_segments) {
  history_based_segments_ = std::move(history_based_segments);
  // If a delete is pending, clear the results now.
  if (pending_deletion_based_on_history_based_segments_) {
    DeleteResultsForHistoryBasedSegments();

    // Only clear results once on first init. This method can be called multiple
    // times during the session when model updates.
    pending_deletion_based_on_history_based_segments_ = false;
  }
}

void HistoryServiceObserver::DeleteResultsForHistoryBasedSegments() {
  if (!history_based_segments_) {
    // Set the delete flag to clear the history based results when
    // SetHistoryBasedSegments() is called.
    pending_deletion_based_on_history_based_segments_ = true;
    return;
  }
  for (const auto segment_id : *history_based_segments_) {
    // For Server models.
    storage_service_->segment_info_database()->SaveSegmentResult(
        segment_id, proto::ModelSource::SERVER_MODEL_SOURCE, std::nullopt,
        base::DoNothing());

    // For Default models.
    storage_service_->segment_info_database()->SaveSegmentResult(
        segment_id, proto::ModelSource::DEFAULT_MODEL_SOURCE, std::nullopt,
        base::DoNothing());
  }

  // If a model refresh was recently posted, then cancel the task and restart
  // the 30 second timer. This is to avoid running models often when user clears
  // multiple history entries at once.
  if (posted_model_refresh_task_) {
    posted_model_refresh_task_->Cancel();
  }
  posted_model_refresh_task_ = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce(models_refresh_callback_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, posted_model_refresh_task_->callback(), base::Minutes(1));
}

}  // namespace segmentation_platform
