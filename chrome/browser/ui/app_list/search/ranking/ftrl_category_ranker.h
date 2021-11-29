// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_FTRL_CATEGORY_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_FTRL_CATEGORY_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

namespace app_list {

// A ranker for categories using a Follow the Regularized Leader algorithm.
//
// TODO(crbug.com/1199206): WIP.
class FtrlCategoryRanker : public Ranker {
 public:
  FtrlCategoryRanker();
  ~FtrlCategoryRanker() override;

  FtrlCategoryRanker(const FtrlCategoryRanker&) = delete;
  FtrlCategoryRanker& operator=(const FtrlCategoryRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;
  void Train(const LaunchData& launch) override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_FTRL_CATEGORY_RANKER_H_
