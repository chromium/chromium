// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"

#include "chrome/browser/ui/app_list/search/ranking/filtering_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/ftrl_category_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/ftrl_result_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results.pb.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/score_normalizing_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/top_match_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/util/score_normalizer.pb.h"

namespace app_list {
namespace {

// A standard write delay used for protos without time-sensitive writes. This is
// intended to be slightly longer than the longest conceivable latency for a
// search.
constexpr base::TimeDelta kStandardWriteDelay = base::Seconds(3);

// No write delay for protos with time-sensitive writes.
constexpr base::TimeDelta kNoWriteDelay = base::Seconds(0);

}  // namespace

RankerDelegate::RankerDelegate(Profile* profile, SearchController* controller) {
  const auto state_dir = RankerStateDirectory(profile);

  // Main result and category ranking.
  AddRanker(std::make_unique<ScoreNormalizingRanker>(
      PersistentProto<ScoreNormalizerProto>(
          state_dir.AppendASCII("score_norm.pb"), kStandardWriteDelay)));

  // Result post-processing.
  AddRanker(std::make_unique<TopMatchRanker>());
  AddRanker(std::make_unique<FilteringRanker>());

  // Result removal.
  AddRanker(std::make_unique<RemovedResultsRanker>(
      PersistentProto<RemovedResultsProto>(
          state_dir.AppendASCII("removed_results.pb"), kNoWriteDelay)));
}

RankerDelegate::~RankerDelegate() {}

void RankerDelegate::Start(const std::u16string& query,
                           ResultsMap& results,
                           CategoriesList& categories) {
  for (auto& ranker : rankers_)
    ranker->Start(query, results, categories);
}

void RankerDelegate::UpdateResultRanks(ResultsMap& results,
                                       ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->UpdateResultRanks(results, provider);
}

void RankerDelegate::UpdateCategoryRanks(const ResultsMap& results,
                                         CategoriesList& categories,
                                         ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->UpdateCategoryRanks(results, categories, provider);
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
