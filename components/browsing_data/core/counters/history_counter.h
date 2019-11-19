// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_

#include <memory>

#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/sync_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/sync/driver/sync_service.h"

namespace browsing_data {

class HistoryCounter : public browsing_data::BrowsingDataCounter {
 public:
  typedef base::Callback<history::WebHistoryService*()>
      GetUpdatedWebHistoryServiceCallback;

  class HistoryResult : public SyncResult {
   public:
    HistoryResult(const HistoryCounter* source,
                  ResultInt value,
                  bool is_sync_enabled,
                  bool has_synced_visits);
    ~HistoryResult() override;

    bool has_synced_visits() const { return has_synced_visits_; }

   private:
    bool has_synced_visits_;
  };

  explicit HistoryCounter(history::HistoryService* history_service,
                          const GetUpdatedWebHistoryServiceCallback& callback,
                          syncer::SyncService* sync_service);
  ~HistoryCounter() override;

  void OnInitialized() override;

  // Whether there are counting tasks in progress. Only used for testing.
  bool HasTrackedTasks();

  const char* GetPrefName() const override;

 private:
  void Count() override;

  void OnGetLocalHistoryCount(history::HistoryCountResult result);
  void OnGetWebHistoryCount(history::WebHistoryService::Request* request,
                            const base::DictionaryValue* result);
  void OnWebHistoryTimeout();
  void MergeResults();

  history::WebHistoryService* GetWebHistoryService();

  bool IsHistorySyncEnabled(const syncer::SyncService* sync_service);

  history::HistoryService* history_service_;

  GetUpdatedWebHistoryServiceCallback web_history_service_callback_;

  SyncTracker sync_tracker_;

  bool has_synced_visits_;

  bool local_counting_finished_;
  bool web_counting_finished_;

  base::CancelableTaskTracker cancelable_task_tracker_;
  std::unique_ptr<history::WebHistoryService::Request> web_history_request_;
  base::OneShotTimer web_history_timeout_;

  base::ThreadChecker thread_checker_;

  BrowsingDataCounter::ResultInt local_result_;

  base::WeakPtrFactory<HistoryCounter> weak_ptr_factory_{this};
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_HISTORY_COUNTER_H_
