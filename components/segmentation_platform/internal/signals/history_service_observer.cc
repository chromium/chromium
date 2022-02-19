// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/history_service_observer.h"

#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/history_delegate_impl.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"

namespace segmentation_platform {

HistoryServiceObserver::HistoryServiceObserver(
    history::HistoryService* history_service,
    UrlSignalHandler* url_signal_handler)
    : url_signal_handler_(url_signal_handler),
      history_delegate_(
          std::make_unique<HistoryDelegateImpl>(history_service,
                                                url_signal_handler)) {
  history_observation_.Observe(history_service);
}

HistoryServiceObserver::~HistoryServiceObserver() = default;

void HistoryServiceObserver::OnURLVisited(
    history::HistoryService* history_service,
    ui::PageTransition transition,
    const history::URLRow& row,
    const history::RedirectList& redirects,
    base::Time visit_time) {
  url_signal_handler_->OnHistoryVisit(row.url());
  history_delegate_->OnUrlAdded(row.url());
}

void HistoryServiceObserver::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  std::vector<GURL> urls;
  for (const auto& info : deletion_info.deleted_rows())
    urls.push_back(info.url());
  url_signal_handler_->OnUrlsRemovedFromHistory(urls);
  history_delegate_->OnUrlRemoved(urls);
}

}  // namespace segmentation_platform
