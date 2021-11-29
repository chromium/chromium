// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ftrl_category_ranker.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

FtrlCategoryRanker::FtrlCategoryRanker() = default;
FtrlCategoryRanker::~FtrlCategoryRanker() = default;

void FtrlCategoryRanker::Start(const std::u16string& query,
                               ResultsMap& results,
                               CategoriesList& categories) {}

void FtrlCategoryRanker::UpdateResultRanks(ResultsMap& results,
                                           ProviderType provider) {
  // TODO(crbug.com/1199206): WIP.
}

void FtrlCategoryRanker::UpdateCategoryRanks(const ResultsMap& results,
                                             CategoriesList& categories,
                                             ProviderType provider) {
  // TODO(crbug.com/1199206): WIP.
}

void FtrlCategoryRanker::Train(const LaunchData& launch) {}

}  // namespace app_list
