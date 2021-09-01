// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/category_item_ranker.h"

#include <cmath>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {

CategoryItemRanker::CategoryItemRanker() {}

CategoryItemRanker::~CategoryItemRanker() {}

void CategoryItemRanker::Start(const std::u16string& query) {
  category_scores_.clear();
}

void CategoryItemRanker::Rank(ResultsMap& results, ProviderType provider) {
  UpdateCategoryScore(results, provider);
  RescoreResults(results);
}

void CategoryItemRanker::UpdateCategoryScore(ResultsMap& results,
                                             ProviderType provider) {
  const auto& it = results.find(provider);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    Scoring& scoring = result->scoring();

    // Ignore top match results for the purposes of deciding category scores,
    // because they're displayed outside their category.
    if (scoring.top_match)
      continue;

    const Category category = ResultTypeToCategory(result->result_type());
    const auto& it = category_scores_.find(category);
    if (it != category_scores_.end()) {
      category_scores_[category] =
          std::max(it->second, scoring.normalized_relevance);
    } else {
      category_scores_[category] = scoring.normalized_relevance;
    }
  }
}

void CategoryItemRanker::RescoreResults(ResultsMap& results) {
  // First, sort the category scores in a vector.
  std::vector<std::pair<Category, double>> category_scores_vec(
      category_scores_.begin(), category_scores_.end());
  std::sort(category_scores_vec.begin(), category_scores_vec.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

  // Replace the scores with integer rankings, lowest-scoring category gets the
  // lowest rank.
  for (int i = 0; i < category_scores_vec.size(); ++i)
    category_scores_vec[i].second = i;
  std::map<Category, double> category_scores_map(category_scores_vec.begin(),
                                                 category_scores_vec.end());

  // Adjust the score of each result to bring them into category-order.
  for (const auto& provider_results : results) {
    for (const auto& result : provider_results.second) {
      Scoring& scoring = result->scoring();

      if (scoring.top_match)
        continue;

      const Category category = ResultTypeToCategory(result->result_type());
      scoring.category_item_score = category_scores_map[category];

      // TODO(crbug.com/1199206): This adds some debug information to the result
      // details. Remove once we have explicit categories in the UI.
      const auto details = RemoveDebugPrefix(result->details());
      result->SetDetails(
          base::StrCat({CategoryDebugString(category), details}));
    }
  }
}

}  // namespace app_list
