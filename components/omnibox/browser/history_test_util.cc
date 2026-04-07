// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_test_util.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"

namespace history {

namespace {

void AddFakeURLToHistoryDB(HistoryDatabase* history_db, const URLRow& url_row) {
  base::Time visit_time = url_row.last_visit();
  URLID url_id = history_db->AddURL(url_row);

  auto AddVisit = [&](ui::PageTransition transition,
                      bool incremented_omnibox_typed_score) mutable {
    // Assume earlier visits are at one-day intervals.
    visit_time -= base::Days(1);
    VisitRow row(url_id, visit_time, 0, transition, 1,
                 incremented_omnibox_typed_score, 0);
    row.source = SOURCE_BROWSED;
    history_db->AddVisit(&row);
  };

  // Mark the most recent |test_info.typed_count| visits as typed.
  for (int j = 0; j < url_row.typed_count(); ++j)
    AddVisit(ui::PAGE_TRANSITION_TYPED, true);

  for (int j = url_row.typed_count(); j < url_row.visit_count(); ++j)
    AddVisit(ui::PAGE_TRANSITION_LINK, false);
}

class FillDataTask : public history::HistoryDBTask {
 public:
  FillDataTask(base::span<const URLRow> url_rows,
               base::RepeatingClosure quit_closure)
      : url_rows_(url_rows.begin(), url_rows.end()),
        quit_closure_(std::move(quit_closure)) {}
  ~FillDataTask() override = default;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    for (const auto& url_row : url_rows_) {
      AddFakeURLToHistoryDB(db, url_row);
    }
    return true;
  }

  void DoneRunOnMainThread() override { quit_closure_.Run(); }

 private:
  const std::vector<URLRow> url_rows_;
  const base::RepeatingClosure quit_closure_;
};

}  // namespace

void AddFakeURLsToHistoryService(HistoryService* history_service,
                                 base::span<const URLRow> url_rows) {
  base::CancelableTaskTracker tracker;
  base::RunLoop run_loop;
  history_service->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<FillDataTask>(url_rows, run_loop.QuitClosure()),
      &tracker);
  run_loop.Run();
}

}  // namespace history
