// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_DELEGATE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_DELEGATE_IMPL_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"
#include "url/gurl.h"

namespace history {
class HistoryService;
}  // namespace history

namespace segmentation_platform {
// Provides an API to query history service to check if an URL exists.
class HistoryDelegateImpl : public UrlSignalHandler::HistoryDelegate {
 public:
  HistoryDelegateImpl(history::HistoryService* history_service,
                      UrlSignalHandler* url_signal_handler,
                      const std::string& profile_id);

  ~HistoryDelegateImpl() override;
  HistoryDelegateImpl(const HistoryDelegateImpl&) = delete;
  HistoryDelegateImpl& operator=(const HistoryDelegateImpl&) = delete;

  // Called by history observer when URLs are added/removed in the history
  // database, useful to store a cache of recent visits.
  void OnUrlAdded(const GURL& url);
  void OnUrlRemoved(const std::vector<GURL>& urls);

  // HistoryDelegate impl:
  bool FastCheckUrl(const GURL& url) override;
  void FindUrlInHistory(const GURL& url,
                        UrlSignalHandler::FindCallback callback) override;
  const std::string& profile_id() override;

  // Getters.

 private:
  void OnHistoryQueryResult(UrlId url_id,
                            UrlSignalHandler::FindCallback callback,
                            history::QueryURLResult result);

  raw_ptr<history::HistoryService> history_service_;

  // The task tracker for the HistoryService callbacks, destroyed after
  // observer is unregistered.
  base::CancelableTaskTracker task_tracker_;

  // ProfileId associated with the current profile.
  const std::string profile_id_;

  base::ScopedObservation<UrlSignalHandler, UrlSignalHandler::HistoryDelegate>
      ukm_db_observation_{this};

  // List of URLs visited in the current session.
  // TODO(ssid): This list grows indefnitely, consider having a limit or LRU
  // cache.
  std::unordered_set<UrlId, UrlId::Hasher> cached_history_urls_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HistoryDelegateImpl> weak_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_DELEGATE_IMPL_H_
