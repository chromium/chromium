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
//
// TODO(crbug.com/1199206): This is temporarily unused while being incorporated
// into the FtrlCategoryRanker.
class CategoryItemRanker : public Ranker {
 public:
  CategoryItemRanker() = default;
  ~CategoryItemRanker() override = default;

  CategoryItemRanker(const CategoryItemRanker&) = delete;
  CategoryItemRanker& operator=(const CategoryItemRanker&) = delete;

  // Ranker:
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_ITEM_RANKER_H_
