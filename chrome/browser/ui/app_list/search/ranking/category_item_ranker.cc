// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/category_item_ranker.h"

#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {

void CategoryItemRanker::UpdateCategoryRanks(const ResultsMap& results,
                                             CategoriesList& categories,
                                             ProviderType provider) {
  const auto& it = results.find(provider);
  DCHECK(it != results.end());

  // Populate a map with the current scores.
  base::flat_map<Category, double> high_scores;
  for (const auto& category : categories)
    high_scores[category.category] = category.score;

  // Update it with the scores from new results.
  for (const auto& result : it->second) {
    Scoring& scoring = result->scoring();

    // Ignore best match results for the purposes of deciding category scores,
    // because they're displayed outside their category.
    if (result->best_match())
      continue;

    high_scores[result->category()] =
        std::max(high_scores[result->category()], scoring.normalized_relevance);
  }

  // Update the category objects with the new scores .
  for (auto& category : categories)
    category.score = high_scores[category.category];
}

}  // namespace app_list
