// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/sync_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/sync/service/sync_service.h"

namespace browsing_data {

class HistoryCounter : public browsing_data::BrowsingDataCounter {
 public:
  class HistoryResult : public SyncResult {
   public:
    HistoryResult(const HistoryCounter* source,
                  ResultInt value,
                  bool is_sync_enabled,
                  bool has_synced_visits,
                  std::string last_visited_domain);
    ~HistoryResult() override;

    bool has_synced_visits() const { return has_synced_visits_; }

    const std::string& last_visited_domain() const {
      return last_visited_domain_;
    }

   private:
    bool has_synced_visits_;
    std::string last_visited_domain_;
  };

  explicit HistoryCounter(history::HistoryService* history_service,
                          history::WebHistoryService* web_history_service,
                          syncer::SyncService* sync_service);
  ~HistoryCounter() override;

  void OnInitialized() override;

  // Whether there are counting tasks in progress. Only used for testing.
  bool HasTrackedTasksForTesting();

  const char* GetPrefName() const override;

 private:
  void Count() override;

  void OnGetWebHistoryCount(
      history::WebHistoryService::Request* request,
      base::optional_ref<const history::WebHistoryService::QueryHistoryResult>
          result);
  void OnGetUniqueDomains(history::DomainsVisitedResult result);
  void OnWebHistoryTimeout();
  void MergeResults();

  bool IsHistorySyncEnabled() const;

  raw_ptr<history::HistoryService, DanglingUntriaged> history_service_;

  raw_ptr<history::WebHistoryService> web_history_service_;

  SyncTracker sync_tracker_;

  bool has_synced_visits_;

  bool web_counting_finished_;
  bool domain_fetching_finished_;

  base::CancelableTaskTracker cancelable_task_tracker_;
  std::unique_ptr<history::WebHistoryService::Request> web_history_request_;
  base::OneShotTimer web_history_timeout_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::string last_visited_domain_;
  BrowsingDataCounter::ResultInt unique_domains_result_;

  base::WeakPtrFactory<HistoryCounter> weak_ptr_factory_{this};
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
