// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_URL_TITLE_FETCHER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_URL_TITLE_FETCHER_IMPL_H_

#include "ash/clipboard/clipboard_history_url_title_fetcher.h"

#include "base/task/cancelable_task_tracker.h"

class GURL;

namespace history {
struct QueryURLResult;
}  // namespace history

// Implements the singleton `ClipboardHistoryUrlTitleFetcher`.
class ClipboardHistoryUrlTitleFetcherImpl
    : public ash::ClipboardHistoryUrlTitleFetcher {
 public:
  ClipboardHistoryUrlTitleFetcherImpl();
  ClipboardHistoryUrlTitleFetcherImpl(ClipboardHistoryUrlTitleFetcherImpl&) =
      delete;
  ClipboardHistoryUrlTitleFetcherImpl& operator=(
      ClipboardHistoryUrlTitleFetcherImpl&) = delete;
  ~ClipboardHistoryUrlTitleFetcherImpl() override;

  // ash::ClipboardHistoryUrlTitleFetcher
  void QueryHistory(const GURL& url,
                    OnHistoryQueryCompleteCallback callback) override;

 private:
  // Interprets the `result` from querying the history service and passes the
  // extracted title, if any, to the client via `callback`.
  void OnHistoryQueryComplete(OnHistoryQueryCompleteCallback callback,
                              history::QueryURLResult result) const;

  // Cancels URL title queries if `this` is destroyed.
  base::CancelableTaskTracker task_tracker_;
};

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_URL_TITLE_FETCHER_IMPL_H_
