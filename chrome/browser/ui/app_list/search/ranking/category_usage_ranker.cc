// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/category_usage_ranker.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {
namespace {

std::string CategoryToString(const Category value) {
  return base::NumberToString(static_cast<int>(value));
}

Category StringToCategory(const std::string& value) {
  int number;
  base::StringToInt(value, &number);
  return static_cast<Category>(number);
}

}  // namespace

CategoryUsageRanker::CategoryUsageRanker(Profile* profile) {
  // Initialize category ranking as a most-frecently-used list.
  RecurrenceRankerConfigProto config;
  config.set_min_seconds_between_saves(1u);
  config.set_condition_limit(1u);
  config.set_condition_decay(0.5);
  config.set_target_limit(20);
  config.set_target_decay(0.8);
  config.mutable_predictor()->mutable_default_predictor();

  category_ranker_ = std::make_unique<RecurrenceRanker>(
      "CategoryUsageRanker",
      RankerStateDirectory(profile).AppendASCII("category_ranker.pb"), config,
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile));
}

void CategoryUsageRanker::InitializeCategoryScores() {
  // Set a default ranking for categories by recording every category once. The
  // |category_ranker_| has a small recency component to its scores, so the
  // categories will appear in reverse order to the Record calls. This must
  // record every category.
  category_ranker_->Record(CategoryToString(Category::kAssistant));
  category_ranker_->Record(CategoryToString(Category::kPlayStore));
  category_ranker_->Record(CategoryToString(Category::kWeb));
  category_ranker_->Record(CategoryToString(Category::kFiles));
  category_ranker_->Record(CategoryToString(Category::kHelp));
  category_ranker_->Record(CategoryToString(Category::kSettings));
  category_ranker_->Record(CategoryToString(Category::kApp));

  // Check we've recorded every category.
  DCHECK_EQ(category_ranker_->Rank().size(),
            static_cast<size_t>(Category::kMaxValue));
}

CategoryUsageRanker::~CategoryUsageRanker() {}

void CategoryUsageRanker::Start(const std::u16string& query) {
  if (!category_ranker_->is_initialized())
    return;

  // If the ranker is empty, seed it with a default ordering for each category.
  if (category_ranker_->empty())
    InitializeCategoryScores();

  // Retrieve all category scores, sorted low-to-high.
  const auto category_scores = category_ranker_->RankSorted();

  // TODO(crbug.com/1199206): Remove this debug output once we no longer need to
  // inspect launcher scores so closely, and before the categorical search flag
  // is enabled for any users.
  LOG(ERROR) << "(categorical search) category scores are:";
  for (const auto& category_score : category_scores) {
    LOG(ERROR) << "(categorical search) - " << category_score.first << " : "
               << category_score.second;
  }

  // Convert the scores into ranks.
  category_ranks_.clear();
  for (int i = 0; i < category_scores.size(); ++i) {
    const auto category = StringToCategory(category_scores[i].first);
    category_ranks_[category] = category_scores.size() - i;
  }
}

void CategoryUsageRanker::Rank(ResultsMap& results, ProviderType provider) {
  // Update each result's score to be:
  //  kCategoryScoreFactor*(category rank) + (result relevance)
  const auto it = results.find(provider);
  DCHECK(it != results.end());
  for (const auto& result : it->second) {
    const auto category = ResultTypeToCategory(result->result_type());
    result->scoring().category_usage_score = category_ranks_[category];

    // TODO(crbug.com/1199206): This adds some debug information to the result
    // details. Remove once we have explicit categories in the UI.
    const auto details = RemoveDebugPrefix(result->details());
    result->SetDetails(base::StrCat({CategoryDebugString(category), details}));
  }
}

void CategoryUsageRanker::Train(const LaunchData& launch) {
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    return;
  }

  category_ranker_->Record(
      CategoryToString(ResultTypeToCategory(launch.result_type)));
}

}  // namespace app_list
