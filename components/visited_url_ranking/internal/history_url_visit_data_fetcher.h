// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_HISTORY_URL_VISIT_DATA_FETCHER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_HISTORY_URL_VISIT_DATA_FETCHER_H_

#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"

namespace history {
struct AnnotatedVisit;
class HistoryService;
}  // namespace history

namespace visited_url_ranking {

// Fetches URL visit data from the history service.
class HistoryURLVisitDataFetcher : public URLVisitDataFetcher {
 public:
  explicit HistoryURLVisitDataFetcher(
      base::WeakPtr<history::HistoryService> history_service);
  HistoryURLVisitDataFetcher(const HistoryURLVisitDataFetcher&) = delete;
  ~HistoryURLVisitDataFetcher() override;

  // URLVisitDataFetcher::
  void FetchURLVisitData(const FetchOptions& options,
                         FetchResultCallback callback) override;

 private:
  // Callback invoked when `AnnotatedVisit` data is ready.
  void OnGotAnnotatedVisits(
      FetchResultCallback callback,
      FetchOptions::FetchSources requested_fetch_sources,
      std::vector<history::AnnotatedVisit> annotated_visits);

  const base::WeakPtr<history::HistoryService> history_service_;

  // The task tracker for the HistoryService callbacks.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<HistoryURLVisitDataFetcher> weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_URL_VISIT_HISTORY_URL_VISIT_DATA_FETCHER_H_
