// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ftrl_result_ranker.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

FtrlResultRanker::FtrlResultRanker() = default;
FtrlResultRanker::~FtrlResultRanker() = default;

void FtrlResultRanker::Start(const std::u16string& query,
                             ResultsMap& results,
                             CategoriesList& categories) {}

void FtrlResultRanker::UpdateResultRanks(ResultsMap& results,
                                         ProviderType provider) {
  // TODO(crbug.com/1199206): WIP.
}

void FtrlResultRanker::UpdateCategoryRanks(const ResultsMap& results,
                                           CategoriesList& categories,
                                           ProviderType provider) {
  // TODO(crbug.com/1199206): WIP.
}

void FtrlResultRanker::Train(const LaunchData& launch) {}

}  // namespace app_list
