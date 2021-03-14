// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback.h"
#include "base/macros.h"

class ChromeSearchResult;

namespace app_list {

enum class RankingItemType;

class SearchProvider {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;
  using ResultChangedCallback = base::RepeatingClosure;

  SearchProvider();
  virtual ~SearchProvider();

  // Invoked to start a query.
  virtual void Start(const std::u16string& query) = 0;
  // Invoked when the UI view closes. In response, the |SearchProvider| may
  // clear its caches.
  virtual void ViewClosing() {}
  // Handles training signals if necessary. A given |SearchProvider| may receive
  // training signals for results of any |RankingItemType|, so it is the
  // |SearchProvider|'s responsibility to check |type| and ignore if necessary.
  virtual void Train(const std::string& id, RankingItemType type) {}
  // Invoked when the app list is shown. This can optionally be used by a
  // provider to eg. warm up a cache of results.
  virtual void AppListShown() {}
  // Returns the main result type created by this provider.
  virtual ash::AppListSearchResultType ResultType() = 0;

  void set_result_changed_callback(ResultChangedCallback callback) {
    result_changed_callback_ = std::move(callback);
  }

  const Results& results() const { return results_; }

 protected:
  // Interface for the derived class to generate search results.
  void Add(std::unique_ptr<ChromeSearchResult> result);

  // Swaps the internal results with |new_results|.
  // This is useful when multiple results will be added, and the notification is
  // desired to be done only once when all results are added.
  void SwapResults(Results* new_results);

  // Clear results and call the |result_changed_callback_|.
  void ClearResults();

  // Clear the results without calling the |result_changed_callback_|.
  void ClearResultsSilently();

 private:
  void FireResultChanged();

  ResultChangedCallback result_changed_callback_;
  Results results_;

  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_PROVIDER_H_
