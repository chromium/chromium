// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_LINK_SUGGESTER_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_LINK_SUGGESTER_H_

#include <vector>

#include "ash/picker/picker_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"

class Profile;

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace history {
class HistoryService;
class QueryResults;
class URLResult;
}  // namespace history

// A class to suggest links based on recent browsing history.
class PickerLinkSuggester {
 public:
  using SuggestedLinksCallback =
      base::RepeatingCallback<void(std::vector<ash::PickerSearchResult>)>;

  explicit PickerLinkSuggester(Profile* profile);
  ~PickerLinkSuggester();
  PickerLinkSuggester(const PickerLinkSuggester&) = delete;
  PickerLinkSuggester& operator=(const PickerLinkSuggester&) = delete;

  void GetSuggestedLinks(size_t max_links, SuggestedLinksCallback callback);

  void set_favicon_service_for_test(favicon::FaviconService* service) {
    favicon_service_ = service;
  }

 private:
  void OnGetBrowsingHistory(SuggestedLinksCallback callback,
                            history::QueryResults results);
  void OnGetFaviconImage(
      history::URLResult result,
      base::OnceCallback<void(ash::PickerSearchResult)> callback,
      const favicon_base::FaviconImageResult& favicon_image_result);

  raw_ptr<history::HistoryService> history_service_;
  base::CancelableTaskTracker history_query_tracker_;
  raw_ptr<favicon::FaviconService> favicon_service_;
  std::vector<base::CancelableTaskTracker> favicon_query_trackers_;
  std::vector<ash::PickerSearchResult> suggested_links_;

  base::WeakPtrFactory<PickerLinkSuggester> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_LINK_SUGGESTER_H_
