// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <string>

namespace app_list {

// Interface for all kinds of rankers. These are ultiamtely owned and called by
// SearchController.
class Ranker {
 public:
  Ranker() = default;
  virtual ~Ranker() = default;

  Ranker(const Ranker&) = delete;
  Ranker& operator=(const Ranker&) = delete;

  // Called each time a new search 'session' begins, eg. when the user opens the
  // launcher or changes the query.
  virtual void Start(const std::u16string& query,
                     ResultsMap& results,
                     CategoriesList& categories);

  // Ranks search results. It may:
  // - return a vector of scores the same length as results[provider].
  // - return nullopt, and directly modify the Scoring struct on search results.
  // Implementations must not modify the input vectors themselves.
  virtual absl::optional<std::vector<double>> RankResults(
      ResultsMap& results,
      CategoriesList& categories,
      ProviderType provider);

  // Ranks categories. It may:
  // - return a vector of scores the same length as |categories|.
  // - return nullopt, and directly modify scores on categories.
  // Implementations must not modify the input vectors themselves.
  virtual absl::optional<std::vector<double>> RankCategories(
      ResultsMap& results,
      CategoriesList& categories,
      ProviderType provider);

  // Called each time a user launches a result.
  virtual void Train(const LaunchData& launch);

  // Called each time a user removes a result.
  virtual void Remove(ChromeSearchResult* result);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_RANKER_H_
