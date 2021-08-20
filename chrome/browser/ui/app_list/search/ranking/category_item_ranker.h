// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_ITEM_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_ITEM_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

namespace app_list {

// A ranker that groups results into categories. Categories are ordered based on
// the highest scoring result per-category. This is assumed to run after the
// score normalization stage, which makes score comparable between search
// providers.
class CategoryItemRanker : public Ranker {
 public:
  CategoryItemRanker();
  ~CategoryItemRanker() override;

  CategoryItemRanker(const CategoryItemRanker&) = delete;
  CategoryItemRanker& operator=(const CategoryItemRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query) override;
  void Rank(ResultsMap& results, ProviderType provider) override;

 private:
  // Updates the score of |provider|'s category in |category_scores_|.
  void UpdateCategoryScore(ResultsMap& results, ProviderType provider);

  // Rescores all current |results| to order them into their categories based on
  // |category_scores_|.
  void RescoreResults(ResultsMap& results);

  std::map<Category, double> category_scores_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_ITEM_RANKER_H_
