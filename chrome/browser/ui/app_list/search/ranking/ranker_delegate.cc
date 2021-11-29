// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"

#include "chrome/browser/ui/app_list/search/ranking/filtering_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/ftrl_category_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/ftrl_result_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/score_normalizing_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/top_match_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {

RankerDelegate::RankerDelegate(Profile* profile, SearchController* controller) {
  AddRanker(std::make_unique<ScoreNormalizingRanker>(profile));
  AddRanker(std::make_unique<FtrlResultRanker>());
  AddRanker(std::make_unique<FtrlCategoryRanker>());
  AddRanker(std::make_unique<TopMatchRanker>());
  AddRanker(std::make_unique<FilteringRanker>());
  AddRanker(std::make_unique<RemovedResultsRanker>(
      RankerStateDirectory(profile).AppendASCII("removed_results_ranker.pb"),
      base::Seconds(30)));
}

RankerDelegate::~RankerDelegate() {}

void RankerDelegate::Start(const std::u16string& query,
                           ResultsMap& results,
                           CategoriesList& categories) {
  for (auto& ranker : rankers_)
    ranker->Start(query, results, categories);
}

absl::optional<std::vector<double>> RankerDelegate::RankResults(
    ResultsMap& results,
    CategoriesList& categories,
    ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->RankResults(results, categories, provider);
  return absl::nullopt;
}

absl::optional<std::vector<double>> RankerDelegate::RankCategories(
    ResultsMap& results,
    CategoriesList& categories,
    ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->RankCategories(results, categories, provider);
  return absl::nullopt;
}

void RankerDelegate::Train(const LaunchData& launch) {
  for (auto& ranker : rankers_)
    ranker->Train(launch);
}

void RankerDelegate::Remove(ChromeSearchResult* result) {
  for (auto& ranker : rankers_)
    ranker->Remove(result);
}

void RankerDelegate::AddRanker(std::unique_ptr<Ranker> ranker) {
  rankers_.emplace_back(std::move(ranker));
}

}  // namespace app_list
