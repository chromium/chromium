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
#include "chrome/browser/ui/app_list/search/ranking/mrfu_cache.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {
namespace {

// This array sets default category scores for each search session. On each call
// to Start:
// - the CategoriesMap is populated with these scores,
// - which are then overridden by any scores in the MRFU cache tracking usage.
//
// This ensures the CategoriesMap always contains a score for all categories.
// The ordering in the array isn't important, but the scores are:
// - they define the OOBE category ranking in the launcher.
// - they are set intentionally low, so that any launcher usage puts a category
//   above the defaults.
constexpr std::array<std::pair<Category, float>, 8> kDefaultScores = {
    std::make_pair(Category::kHelp, 8e-5f),
    std::make_pair(Category::kApps, 7e-5f),
    std::make_pair(Category::kAppShortcuts, 6e-5f),
    std::make_pair(Category::kWeb, 5e-5f),
    std::make_pair(Category::kFiles, 4e-5f),
    std::make_pair(Category::kSettings, 3e-5f),
    std::make_pair(Category::kPlayStore, 2e-5f),
    std::make_pair(Category::kSearchAndAssistant, 1e-5f),
};

constexpr double kEps = 1.0e-5;

std::string CategoryToString(const Category value) {
  return base::NumberToString(static_cast<int>(value));
}

Category StringToCategory(const std::string& value) {
  int number;
  base::StringToInt(value, &number);
  return static_cast<Category>(number);
}

void AddDefaultCategoryScores(CategoriesMap& categories) {
  for (const auto& category_score : kDefaultScores) {
    categories.emplace(category_score.first, category_score.second);
  }

  // Ensure we've recorded every category except unknown.
  DCHECK_EQ(categories.size(), static_cast<size_t>(Category::kMaxValue));
}

}  // namespace

CategoryUsageRanker::CategoryUsageRanker(Profile* profile) {
  // Initialize category ranking as a most-frecently-used list.
  MrfuCache::Params params;
  // Set max params to more than the maximum number of categories
  params.max_items = 50;

  // TODO(crbug.com/1199206): Tweak these settings after manual testing.
  params.half_life = 10.0f;
  params.boost_factor = 10.0f;

  ranker_ = std::make_unique<MrfuCache>(
      RankerStateDirectory(profile).AppendASCII("category_usage_ranker.pb"),
      params);
}

CategoryUsageRanker::~CategoryUsageRanker() {}

void CategoryUsageRanker::Start(const std::u16string& query,
                                ResultsMap& results,
                                CategoriesMap& categories) {
  AddDefaultCategoryScores(categories);

  if (!ranker_->initialized()) {
    return;
  }

  // Retrieve all category scores, sorted high-to-low, and set category ranks
  // based on their ordering.
  // TODO(crbug.com/1199206): The kEps is useful for debugging here, but can be
  // removed once no longer needed.
  auto category_scores = ranker_->GetAll();
  MrfuCache::Sort(category_scores);
  for (int i = 0; i < category_scores.size(); ++i) {
    const auto category = StringToCategory(category_scores[i].first);
    categories[category] = category_scores.size() - i * kEps;
  }
}

void CategoryUsageRanker::Rank(ResultsMap& results,
                               CategoriesMap& categories,
                               ProviderType provider) {
  // TODO(crbug.com/1199206): This adds some debug information to the result
  // details. Remove once we have explicit categories in the UI.
  const auto it = results.find(provider);
  DCHECK(it != results.end());
  for (const auto& result : it->second) {
    const auto category = ResultTypeToCategory(result->result_type());
    const auto details = RemoveDebugPrefix(result->details());
    result->SetDetails(base::StrCat({CategoryDebugString(category), details}));
  }
}

void CategoryUsageRanker::Train(const LaunchData& launch) {
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    return;
  }

  // TODO(crbug.com/1199206): Update launch data to include the category
  // directly, then delete ResultTypeToCategory.
  ranker_->Use(CategoryToString(ResultTypeToCategory(launch.result_type)));
}

}  // namespace app_list
