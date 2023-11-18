// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/history_delegate_impl.h"

#include "base/hash/hash.h"
#include "components/history/core/browser/history_service.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"

namespace segmentation_platform {

HistoryDelegateImpl::HistoryDelegateImpl(
    history::HistoryService* history_service,
    UrlSignalHandler* url_signal_handler,
    const std::string& profile_id)
    : history_service_(history_service), profile_id_(profile_id) {
  ukm_db_observation_.Observe(url_signal_handler);
}

HistoryDelegateImpl::~HistoryDelegateImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HistoryDelegateImpl::OnUrlAdded(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cached_history_urls_.insert(UkmUrlTable::GenerateUrlId(url));
}

void HistoryDelegateImpl::OnUrlRemoved(const std::vector<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const GURL& url : urls) {
    cached_history_urls_.erase(UkmUrlTable::GenerateUrlId(url));
  }
}

bool HistoryDelegateImpl::FastCheckUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_history_urls_.count(UkmUrlTable::GenerateUrlId(url));
}

void HistoryDelegateImpl::FindUrlInHistory(
    const GURL& url,
    UrlSignalHandler::FindCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  history_service_->QueryURL(
      url, /* want_visits=*/false,
      base::BindOnce(&HistoryDelegateImpl::OnHistoryQueryResult,
                     weak_factory_.GetWeakPtr(),
                     UkmUrlTable::GenerateUrlId(url), std::move(callback)),
      &task_tracker_);
}

const std::string& HistoryDelegateImpl::profile_id() {
  return profile_id_;
}

void HistoryDelegateImpl::OnHistoryQueryResult(
    UrlId url_id,
    UrlSignalHandler::FindCallback callback,
    history::QueryURLResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.success) {
    cached_history_urls_.insert(url_id);
  }
  std::move(callback).Run(result.success, result.success ? profile_id_ : "");
}

}  // namespace segmentation_platform
