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

// Note: Results and ResultsMap should be defined here, but are defined in
// SearchController to avoid an include loop.

// All score information for a single result. This is stored with a result, and
// incrementally updated by rankers as needed. Generally, each ranker should
// control one score.
//
// TODO(crbug.com/1199206): Remove defunct members from this struct.
struct Scoring {
  bool filter = false;
  // The rank (0, 1, 2, ...) of this result within the Best Match collection of
  // results, or -1 if this result is not a Best Match.
  int best_match_rank = -1;
  double normalized_relevance = 0.0f;
  double category_item_score = 0.0f;
  double category_usage_score = 0.0f;
  double ftrl_result_score = 0.0;
  double usage_score = 0.0f;

  // A counter for the burn-in iteration number, where 0 signifies the
  // pre-burn-in state, and 1 and above signify the post-burn-in state.
  // Incremented during the post-burn-in period each time a provider
  // returns. Not applicable to zero-state search.
  int burnin_iteration = 0;

  Scoring() {}

  Scoring(const Scoring&) = delete;
  Scoring& operator=(const Scoring&) = delete;

  double FinalScore() const;
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
