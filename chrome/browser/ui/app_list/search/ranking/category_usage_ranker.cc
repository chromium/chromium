// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/category_usage_ranker.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/util/mrfu_cache.h"

namespace app_list {
namespace {

// This map sets default category scores for each search session. On each call
// to Start:
// - the CategoriesList is populated with these scores,
// - which are then overridden by any scores in the MRFU cache tracking usage.
//
// This ensures the CategoriesList always contains a non-zero score for all
// categories. This defines the OOBE  category ranking in the launcher. Scores
// are set intentionally low, so that any launcher usage puts a category above
// the defaults.
constexpr auto kDefaultScores = base::MakeFixedFlatMap<Category, double>(
    {{Category::kHelp, 8e-5},
     {Category::kApps, 7e-5},
     {Category::kAppShortcuts, 6e-5},
     {Category::kWeb, 5e-5},
     {Category::kFiles, 4e-5},
     {Category::kSettings, 3e-5},
     {Category::kPlayStore, 2e-5},
     {Category::kSearchAndAssistant, 1e-5}});

constexpr double kEps = 1.0e-5;

std::string CategoryToString(const Category value) {
  return base::NumberToString(static_cast<int>(value));
}

Category StringToCategory(const std::string& value) {
  int number;
  base::StringToInt(value, &number);
  return static_cast<Category>(number);
}

void SetDefaultCategoryScores(CategoriesList& categories) {
  for (auto& category : categories) {
    auto* it = kDefaultScores.find(category.category);
    // Insert the default score if no other score exists.
    if (it == kDefaultScores.end())
      category.score = it->second;
  }
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

  MrfuCache::Proto proto(
      RankerStateDirectory(profile).AppendASCII("category_usage_ranker.pb"),
      /*write_delay=*/base::Seconds(3));
  ranker_ = std::make_unique<MrfuCache>(std::move(proto), params);
}

CategoryUsageRanker::~CategoryUsageRanker() {}

void CategoryUsageRanker::Start(const std::u16string& query,
                                ResultsMap& results,
                                CategoriesList& categories) {
  // Ensure all categories are accounted for in the input.
  DCHECK_EQ(categories.size(), static_cast<size_t>(Category::kMaxValue));
  SetDefaultCategoryScores(categories);

  if (!ranker_->initialized())
    return;

  // Retrieve all category scores, sorted high-to-low, and set category ranks
  // based on their ordering.
  // TODO(crbug.com/1199206): The kEps is useful for debugging here, but can be
  // removed once no longer needed.
  auto category_scores = ranker_->GetAll();
  MrfuCache::Sort(category_scores);
  for (int i = 0; i < category_scores.size(); ++i) {
    const auto category_value = StringToCategory(category_scores[i].first);
    for (auto& category : categories) {
      if (category.category == category_value)
        category.score = category_scores[i].second - i * kEps;
    }
  }
}

void CategoryUsageRanker::UpdateCategoryRanks(const ResultsMap& results,
                                              CategoriesList& categories,
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
