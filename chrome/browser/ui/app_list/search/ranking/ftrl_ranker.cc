// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ftrl_ranker.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/util/ftrl_optimizer.h"

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

// TODO(crbug.com/1199206): This can be removed once LaunchData contains the
// result category.
//
// This is slightly inconsistent with the true result->category mapping, because
// Omnibox results can either be in the kWeb or kSearchAndAssistant category.
Category ResultTypeToCategory(ResultType result_type) {
  switch (result_type) {
    case ResultType::kInstalledApp:
    case ResultType::kInstantApp:
    case ResultType::kInternalApp:
      return Category::kApps;
    case ResultType::kArcAppShortcut:
      return Category::kAppShortcuts;
    case ResultType::kOmnibox:
    case ResultType::kAnswerCard:
    case ResultType::kOpenTab:
      return Category::kWeb;
    case ResultType::kZeroStateFile:
    case ResultType::kZeroStateDrive:
    case ResultType::kFileChip:
    case ResultType::kDriveChip:
    case ResultType::kFileSearch:
    case ResultType::kDriveSearch:
      return Category::kFiles;
    case ResultType::kOsSettings:
      return Category::kSettings;
    case ResultType::kHelpApp:
    case ResultType::kKeyboardShortcut:
      return Category::kHelp;
    case ResultType::kPlayStoreReinstallApp:
    case ResultType::kPlayStoreApp:
      return Category::kPlayStore;
    case ResultType::kAssistantChip:
    case ResultType::kAssistantText:
      return Category::kSearchAndAssistant;
    // Never used in the search backend.
    case ResultType::kUnknown:
    // Suggested content toggle fake result type. Used only in ash, not in the
    // search backend.
    case ResultType::kInternalPrivacyInfo:
    // Deprecated.
    case ResultType::kLauncher:
      return Category::kUnknown;
  }
}

}  // namespace

// FtrlRanker ------------------------------------------------------------

FtrlRanker::FtrlRanker(FtrlRanker::RankingKind kind,
                       FtrlOptimizer::Params params,
                       FtrlOptimizer::Proto proto)
    : kind_(kind),
      ftrl_(std::make_unique<FtrlOptimizer>(std::move(proto), params)) {}

FtrlRanker::~FtrlRanker() = default;

void FtrlRanker::AddExpert(std::unique_ptr<Ranker> ranker) {
  rankers_.push_back(std::move(ranker));
}

void FtrlRanker::Start(const std::u16string& query,
                       ResultsMap& results,
                       CategoriesList& categories) {
  for (auto& ranker : rankers_)
    ranker->Start(query, results, categories);
}

void FtrlRanker::Train(const LaunchData& launch) {
  for (auto& ranker : rankers_)
    ranker->Train(launch);

  switch (kind_) {
    case FtrlRanker::RankingKind::kResults:
      ftrl_->Train(launch.id);
      break;
    case FtrlRanker::RankingKind::kCategories:
      ftrl_->Train(CategoryToString(ResultTypeToCategory(launch.result_type)));
      break;
  }
}

void FtrlRanker::UpdateResultRanks(ResultsMap& results, ProviderType provider) {
  if (kind_ != FtrlRanker::RankingKind::kResults)
    return;

  const auto it = results.find(provider);
  DCHECK(it != results.end());
  const auto& new_results = it->second;

  // Create a vector of result ids.
  std::vector<std::string> ids;
  for (const auto& result : new_results)
    ids.push_back(result->id());

  // Create a vector of vectors for each expert's scores.
  std::vector<std::vector<double>> expert_scores;
  for (auto& ranker : rankers_)
    expert_scores.push_back(ranker->GetResultRanks(results, provider));

  // Get the final scores from the FTRL optimizer set them on the results.
  std::vector<double> result_scores =
      ftrl_->Score(std::move(ids), std::move(expert_scores));
  DCHECK_EQ(new_results.size(), result_scores.size());
  for (size_t i = 0; i < new_results.size(); ++i)
    new_results[i]->scoring().ftrl_result_score = result_scores[i];
}

void FtrlRanker::UpdateCategoryRanks(const ResultsMap& results,
                                     CategoriesList& categories,
                                     ProviderType provider) {
  if (kind_ != FtrlRanker::RankingKind::kCategories)
    return;

  // Create a vector of category strings.
  std::vector<std::string> category_strings;
  for (const auto& category : categories)
    category_strings.push_back(CategoryToString(category.category));

  // Create a vector of vectors for each expert's scores.
  std::vector<std::vector<double>> expert_scores;
  for (auto& ranker : rankers_) {
    expert_scores.push_back(
        ranker->GetCategoryRanks(results, categories, provider));
  }

  // Get the final scores from the FTRL optimizer set them on the categories.
  std::vector<double> category_scores =
      ftrl_->Score(std::move(category_strings), std::move(expert_scores));
  DCHECK_EQ(categories.size(), category_scores.size());
  for (size_t i = 0; i < categories.size(); ++i)
    categories[i].score = category_scores[i];
}

// MrfuResultRanker -----------------------------------------------------------

MrfuResultRanker::MrfuResultRanker(MrfuCache::Params params,
                                   MrfuCache::Proto proto)
    : mrfu_(std::make_unique<MrfuCache>(std::move(proto), params)) {}

MrfuResultRanker::~MrfuResultRanker() = default;

std::vector<double> MrfuResultRanker::GetResultRanks(const ResultsMap& results,
                                                     ProviderType provider) {
  const auto it = results.find(provider);
  if (it == results.end())
    return {};

  std::vector<double> scores;
  for (const auto& result : it->second)
    scores.push_back(mrfu_->Get(result->id()));
  return scores;
}

void MrfuResultRanker::Train(const LaunchData& launch) {
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    return;
  }
  mrfu_->Use(launch.id);
}

// NormalizedScoreResultRanker -------------------------------------------------

// Exposes the normalized scores from each result's scoring struct, set by the
// ScoreNormalizingRanker.
std::vector<double> NormalizedScoreResultRanker::GetResultRanks(
    const ResultsMap& results,
    ProviderType provider) {
  const auto it = results.find(provider);
  if (it == results.end())
    return {};

  std::vector<double> scores;
  for (const auto& result : it->second)
    scores.push_back(result->scoring().normalized_relevance);
  return scores;
}

// BestResultCategoryRanker ----------------------------------------------------

BestResultCategoryRanker::BestResultCategoryRanker() = default;
BestResultCategoryRanker::~BestResultCategoryRanker() = default;

void BestResultCategoryRanker::Start(const std::u16string& query,
                                     ResultsMap& results,
                                     CategoriesList& categories) {
  current_category_scores_.clear();
  for (const auto& category : categories)
    current_category_scores_[category.category] = 0.0;
}

std::vector<double> BestResultCategoryRanker::GetCategoryRanks(
    const ResultsMap& results,
    const CategoriesList& categories,
    ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    // Ignore best match, answer card results, and filtered results for the
    // purposes of deciding category scores, because they are not displayed in
    // their category.
    if (result->best_match() || result->scoring().filter ||
        result->display_type() ==
            ChromeSearchResult::DisplayType::kAnswerCard) {
      continue;
    }
    current_category_scores_[result->category()] =
        std::max(result->scoring().normalized_relevance,
                 current_category_scores_[result->category()]);
  }

  // Collect those scores into a vector with the same ordering as |categories|.
  // For categories with no results in them, defaults to 0.0.
  std::vector<double> result;
  for (const auto& category : categories)
    result.push_back(current_category_scores_[category.category]);
  return result;
}

// MrfuCategoryRanker ----------------------------------------------------------

MrfuCategoryRanker::MrfuCategoryRanker(MrfuCache::Params params,
                                       PersistentProto<MrfuCacheProto> proto)
    : mrfu_(std::make_unique<MrfuCache>(std::move(proto), params)) {}

MrfuCategoryRanker::~MrfuCategoryRanker() = default;

void MrfuCategoryRanker::Start(const std::u16string& query,
                               ResultsMap& results,
                               CategoriesList& categories) {
  if (mrfu_->initialized() && mrfu_->empty())
    SetDefaultCategoryScores();
}

std::vector<double> MrfuCategoryRanker::GetCategoryRanks(
    const ResultsMap& results,
    const CategoriesList& categories,
    ProviderType provider) {
  if (!mrfu_->initialized())
    return std::vector<double>(categories.size(), 0.0);

  // Build a map of the MRFU category scores, with 0.0 for unseen categories.
  base::flat_map<Category, double> scores_map;
  for (const auto& id_score : mrfu_->GetAllNormalized())
    scores_map[StringToCategory(id_score.first)] = id_score.second;

  std::vector<double> scores;
  for (const auto& category : categories) {
    const auto it = scores_map.find(category.category);
    scores.push_back(it != scores_map.end() ? it->second : 0.0);
  }
  DCHECK_EQ(scores.size(), categories.size());
  return scores;
}

void MrfuCategoryRanker::UpdateCategoryRanks(const ResultsMap& results,
                                             CategoriesList& categories,
                                             ProviderType provider) {
  const auto& scores = GetCategoryRanks(results, categories, provider);
  DCHECK_EQ(scores.size(), categories.size());
  if (scores.size() != categories.size())
    return;
  for (size_t i = 0; i < categories.size(); ++i)
    categories[i].score = scores[i];
}

void MrfuCategoryRanker::Train(const LaunchData& launch) {
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    return;
  }
  mrfu_->Use(CategoryToString(ResultTypeToCategory(launch.result_type)));
}

void MrfuCategoryRanker::SetDefaultCategoryScores() {
  // TODO(crbug.com/1199206): Implement.
}

}  // namespace app_list
