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

absl::optional<std::vector<double>> FtrlResultRanker::RankResults(
    ResultsMap& results,
    CategoriesList& categories,
    ProviderType provider) {
  return absl::nullopt;
}

absl::optional<std::vector<double>> FtrlResultRanker::RankCategories(
    ResultsMap& results,
    CategoriesList& categories,
    ProviderType provider) {
  return absl::nullopt;
}

void FtrlResultRanker::Train(const LaunchData& launch) {}

}  // namespace app_list
