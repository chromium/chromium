// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

namespace app_list {

// A ranker which removes results which have previously been marked for removal
// from the launcher search results list.
//
// TODO(crbug.com/1263751): Update description when implemented.
class RemovedResultsRanker : public Ranker {
 public:
  RemovedResultsRanker() = default;
  ~RemovedResultsRanker() override = default;

  RemovedResultsRanker(const RemovedResultsRanker&) = delete;
  RemovedResultsRanker& operator=(const RemovedResultsRanker&) = delete;

  // Ranker:
  void Rank(ResultsMap& results,
            CategoriesMap& categories,
            ProviderType provider) override;

  void Remove(ChromeSearchResult* result) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_REMOVED_RESULTS_RANKER_H_
