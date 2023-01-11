// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/url_signal_handler.h"

#include "base/functional/callback.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"

namespace segmentation_platform {

bool UrlSignalHandler::HistoryDelegate::FastCheckUrl(const GURL& url) {
  return false;
}

UrlSignalHandler::UrlSignalHandler(UkmDatabase* ukm_database)
    : ukm_database_(ukm_database) {}

UrlSignalHandler::~UrlSignalHandler() {
  DCHECK(history_delegates_.empty());
}

void UrlSignalHandler::OnUkmSourceUpdated(ukm::SourceId source_id,
                                          const std::vector<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GURL& url = urls.back();
  auto callback = base::BindOnce(&UrlSignalHandler::OnCheckedHistory,
                                 weak_factory_.GetWeakPtr(), source_id, url);
  CheckHistoryForUrl(url, std::move(callback));
}

void UrlSignalHandler::OnHistoryVisit(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ukm_database_->OnUrlValidated(url);
}

void UrlSignalHandler::OnUrlsRemovedFromHistory(const std::vector<GURL>& urls,
                                                bool all_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ukm_database_->RemoveUrls(urls, all_urls);
}

void UrlSignalHandler::AddHistoryDelegate(HistoryDelegate* history_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = history_delegates_.insert(history_delegate);
  DCHECK(it.second);
}

void UrlSignalHandler::RemoveHistoryDelegate(
    HistoryDelegate* history_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto count = history_delegates_.erase(history_delegate);
  DCHECK(count);
}

void UrlSignalHandler::CheckHistoryForUrl(const GURL& url,
                                          FindCallback callback) {
  for (auto& history_delegate : history_delegates_) {
    if (history_delegate->FastCheckUrl(url)) {
      std::move(callback).Run(true);
      return;
    }
  }
  if (skip_history_db_check_) {
    std::move(callback).Run(false);
  } else {
    // TODO(ssid): Add metrics on what percentage of URLs will be missed if we
    // do not do this check, and wait for notifications instead.
    ContinueCheckingHistory(
        url, std::make_unique<base::flat_set<HistoryDelegate*>>(),
        std::move(callback), false);
  }
}

void UrlSignalHandler::ContinueCheckingHistory(
    const GURL& url,
    std::unique_ptr<base::flat_set<HistoryDelegate*>> delegates_checked,
    FindCallback callback,
    bool found) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (found) {
    std::move(callback).Run(true);
    return;
  }

  for (auto& next_delegate : history_delegates_) {
    if (!delegates_checked->count(next_delegate)) {
      delegates_checked->insert(next_delegate);
      next_delegate->FindUrlInHistory(
          url,
          base::BindOnce(&UrlSignalHandler::ContinueCheckingHistory,
                         weak_factory_.GetWeakPtr(), url,
                         std::move(delegates_checked), std::move(callback)));
      return;
    }
  }
  std::move(callback).Run(false);
}

void UrlSignalHandler::OnCheckedHistory(ukm::SourceId source_id,
                                        const GURL& url,
                                        bool in_history) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ukm_database_->UpdateUrlForUkmSource(source_id, url, in_history);
}

}  // namespace segmentation_platform
