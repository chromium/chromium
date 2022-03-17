// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_path.h"

namespace app_list {

// The type of a particular result.
using ResultType = ash::AppListSearchResultType;
// The type of a search provider as a whole. This is currently just the 'main'
// ResultType returned by the provider.
using ProviderType = ash::AppListSearchResultType;
// The display type of a search result, indicating which UI section it belongs
// to.
using DisplayType = ash::SearchResultDisplayType;

// Note: Results and ResultsMap should be defined here, but are defined in
// SearchController to avoid an include loop.

// All score information for a single result. This is stored with a result, and
// is used as 'scratch space' for ranking calculations to pass information
// between rankers. Generally, each member is controlled by one ranker.
struct Scoring {
  // = Members used to compute the display score of a result ===================
  bool filter = false;
  double normalized_relevance = 0.0;
  double mrfu_result_score = 0.0;
  double ftrl_result_score = 0.0;

  // Used only for results in the Continue section. Continue results are first
  // ordered by |continue_rank|, and then by their display score. -1 indicates
  // this is unset.
  int continue_rank = -1;

  // = Members used for sorting in SearchController ============================
  // The rank (0, 1, 2, ...) of this result within
  // the Best Match collection of results, or -1 if this result is not a Best
  // Match.
  int best_match_rank = -1;
  // A counter for the burn-in iteration number, where 0 signifies the
  // pre-burn-in state, and 1 and above signify the post-burn-in state.
  // Incremented during the post-burn-in period each time a provider
  // returns. Not applicable to zero-state search.
  int burnin_iteration = 0;

  Scoring() {}

  Scoring(const Scoring&) = delete;
  Scoring& operator=(const Scoring&) = delete;

  // Score used for ranking within a non-best match category.
  double FinalScore() const;

  // Score used to determine if a result should be considered a best match.
  double BestMatchScore() const;
};

::std::ostream& operator<<(::std::ostream& os, const Scoring& result);

// All possible categories.
using Category = ash::AppListSearchResultCategory;

// A wrapper struct around a category to hold scoring information.
struct CategoryMetadata {
  Category category = Category::kUnknown;
  double score = 0.0;

  // Same purpose, meaning, and incrementing rules as the burnin_iteration
  // member of the Scoring struct above, except this member is for categories
  // rather than individual results. Additionally, -1 signifies that the
  // category has not yet been seen in the current search.
  int burnin_iteration = -1;
};

using CategoriesList = std::vector<CategoryMetadata>;

CategoriesList CreateAllCategories();

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_
