// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_HISTORY_DATA_SOURCE_H_
#define COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_HISTORY_DATA_SOURCE_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

namespace history {
class HistoryService;
}  // namespace history

namespace page_info {

class PageInfoHistoryDataSource {
 public:
  explicit PageInfoHistoryDataSource(history::HistoryService* history_service,
                                     const GURL& site_url);
  ~PageInfoHistoryDataSource();

  // Returns `last_visit` timestamp formatted as "Last visited: `last_visit`"
  // string. If `last_visit` was today or yersterday, 'today' or 'yesterday'
  // will be used. If the visit was this week, 'x days ago' will be used.
  // Otherwise, the short date representation will be used.
  static std::u16string FormatLastVisitedTimestamp(
      base::Time last_visit,
      base::Time now = base::Time::Now());

  // Gets a version of the last time any webpage on the `site_url` host was
  // visited by using the min("last navigation time", x minutes ago) as the
  // upper bound of the GetLastVisitToHost query. This is done in order to
  // provide the user with a more useful sneak peak into their navigation
  // history, by excluding the site(s) they were just on.
  void GetLastVisitedTimestamp(
      base::OnceCallback<void(std::optional<base::Time>)> callback);

 private:
  // Callback from the history system when the last visit query has completed.
  // May need to do a second query based on the results.
  void OnLastVisitBeforeRecentNavigationsComplete(
      const std::string& host_name,
      base::Time query_start_time,
      base::OnceCallback<void(std::optional<base::Time>)> callback,
      history::HistoryLastVisitResult result);

  // Callback from the history system when the last visit query has completed
  // the second time.
  void OnLastVisitBeforeRecentNavigationsComplete2(
      base::OnceCallback<void(std::optional<base::Time>)> callback,
      history::HistoryLastVisitResult result);

  raw_ptr<history::HistoryService> history_service_ = nullptr;

  // Tracker for search requests to the history service.
  base::CancelableTaskTracker query_task_tracker_;

  GURL site_url_;

  base::WeakPtrFactory<PageInfoHistoryDataSource> weak_factory_{this};
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_HISTORY_DATA_SOURCE_H_
