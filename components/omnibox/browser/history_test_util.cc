// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_test_util.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_database.h"

namespace history {

void AddFakeURLToHistoryDB(HistoryDatabase* history_db, const URLRow& url_row) {
  base::Time visit_time = url_row.last_visit();
  URLID url_id = history_db->AddURL(url_row);

  auto AddVisit = [&](ui::PageTransition transition,
                      bool incremented_omnibox_typed_score) mutable {
    // Assume earlier visits are at one-day intervals.
    visit_time -= base::Days(1);
    VisitRow row(url_id, visit_time, 0, transition, 1,
                 incremented_omnibox_typed_score, 0);
    history_db->AddVisit(&row, SOURCE_BROWSED);
  };

  // Mark the most recent |test_info.typed_count| visits as typed.
  for (int j = 0; j < url_row.typed_count(); ++j)
    AddVisit(ui::PAGE_TRANSITION_TYPED, true);

  for (int j = url_row.typed_count(); j < url_row.visit_count(); ++j)
    AddVisit(ui::PAGE_TRANSITION_LINK, false);
}

}  // namespace history
