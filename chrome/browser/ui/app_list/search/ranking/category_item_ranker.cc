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

void CategoryItemRanker::Rank(ResultsMap& results,
                              CategoriesMap& categories,
                              ProviderType provider) {
  const auto& it = results.find(provider);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    Scoring& scoring = result->scoring();

    // Ignore best match results for the purposes of deciding category scores,
    // because they're displayed outside their category.
    if (result->best_match()) {
      continue;
    }

    const Category category = result->category();
    const auto& it = categories.find(category);
    if (it != categories.end()) {
      categories[category] = std::max(it->second, scoring.normalized_relevance);
    } else {
      categories[category] = scoring.normalized_relevance;
    }
  }
}

}  // namespace app_list
