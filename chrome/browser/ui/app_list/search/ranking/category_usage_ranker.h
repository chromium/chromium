// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_USAGE_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_USAGE_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

class Profile;

namespace app_list {

class MrfuCache;

// A ranker that groups results into categories.
//
// TODO(crbug.com/1199206): This is temporarily unused while being incorporated
// into the FtrlCategoryRanker.
class CategoryUsageRanker : public Ranker {
 public:
  explicit CategoryUsageRanker(Profile* profile);
  ~CategoryUsageRanker() override;

  CategoryUsageRanker(const CategoryUsageRanker&) = delete;
  CategoryUsageRanker& operator=(const CategoryUsageRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  std::unique_ptr<MrfuCache> ranker_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_USAGE_RANKER_H_
