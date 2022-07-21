// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_provider.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/games/game_result.h"
#include "chrome/browser/ui/app_list/search/search_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace {

using chromeos::string_matching::FuzzyTokenizedStringMatch;
using chromeos::string_matching::TokenizedString;

// Parameters for FuzzyTokenizedStringMatch.
constexpr bool kUseWeightedRatio = false;
constexpr bool kUseEditDistance = false;
constexpr double kRelevanceThreshold = 0.32;
constexpr double kPartialMatchPenaltyRate = 0.9;

constexpr size_t kMaxResults = 3u;

// Outcome of a call to GameSearchProvider::Start. These values persist to logs.
// Entries should not be renumbered and numeric values should not be reused.
enum class Status {
  kOk = 0,
  kDisabledByPolicy = 1,
  kEmptyIndex = 2,
  kMaxValue = kEmptyIndex,
};

void LogStatus(Status status) {
  base::UmaHistogramEnumeration("Apps.AppList.GameProvider.SearchStatus",
                                status);
}

void LogUpdateStatus(apps::DiscoveryError status) {
  base::UmaHistogramEnumeration("Apps.AppList.GameProvider.UpdateStatus",
                                status);
}

bool EnabledByPolicy(Profile* profile) {
  bool enabled_override = base::GetFieldTrialParamByFeatureAsBool(
      search_features::kLauncherGameSearch, "enabled_override",
      /*default_value=*/false);
  if (enabled_override)
    return true;

  bool suggested_content_enabled =
      profile->GetPrefs()->GetBoolean(ash::prefs::kSuggestedContentEnabled);
  return suggested_content_enabled;
}

double CalculateTitleRelevance(const TokenizedString& tokenized_query,
                               const std::u16string& game_title) {
  const TokenizedString tokenized_title(game_title,
                                        TokenizedString::Mode::kCamelCase);

  if (tokenized_query.text().empty() || tokenized_title.text().empty()) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  FuzzyTokenizedStringMatch match;
  return match.Relevance(tokenized_query, tokenized_title, kUseWeightedRatio,
                         kUseEditDistance, kPartialMatchPenaltyRate);
}

std::vector<std::pair<const apps::Result*, double>> SearchGames(
    const std::u16string& query,
    const GameProvider::GameIndex* index) {
  DCHECK(index);

  TokenizedString tokenized_query(query, TokenizedString::Mode::kCamelCase);
  std::vector<std::pair<const apps::Result*, double>> matches;
  for (const auto& game : *index) {
    double relevance =
        CalculateTitleRelevance(tokenized_query, game.GetAppTitle());
    if (relevance > kRelevanceThreshold) {
      matches.push_back(std::make_pair(&game, relevance));
    }
  }
  return matches;
}

}  // namespace

GameProvider::GameProvider(Profile* profile,
                           AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller),
      app_discovery_service_(
          apps::AppDiscoveryServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This call will fail if the app discovery service has not finished
  // initializing. In that case, we will update when notified via the
  // subscription.
  UpdateIndex();

  DCHECK(app_discovery_service_);
  // It's safe to use an unretained pointer here due to the nature of
  // CallbackListSubscription.
  subscription_ = app_discovery_service_->RegisterForAppUpdates(
      apps::ResultType::kGameSearchCatalog,
      base::BindRepeating(&GameProvider::OnIndexUpdatedBySubscription,
                          base::Unretained(this)));
}

GameProvider::~GameProvider() = default;

ash::AppListSearchResultType GameProvider::ResultType() const {
  return ash::AppListSearchResultType::kGames;
}

void GameProvider::UpdateIndex() {
  app_discovery_service_->GetApps(apps::ResultType::kGameSearchCatalog,
                                  base::BindOnce(&GameProvider::OnIndexUpdated,
                                                 weak_factory_.GetWeakPtr()));
}

void GameProvider::OnIndexUpdated(const GameIndex& index,
                                  apps::DiscoveryError error) {
  LogUpdateStatus(error);
  if (!index.empty())
    game_index_ = index;
}

void GameProvider::OnIndexUpdatedBySubscription(const GameIndex& index) {
  // TODO(crbug.com/1305880): Add tests to check that this is called when the
  // app discovery service notifies its subscribers.
  if (!index.empty())
    game_index_ = index;
}

void GameProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnabledByPolicy(profile_)) {
    LogStatus(Status::kDisabledByPolicy);
    return;
  } else if (game_index_.empty()) {
    LogStatus(Status::kEmptyIndex);
    return;
  }

  // Clear results and discard any existing searches.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&SearchGames, query, &game_index_),
      base::BindOnce(&GameProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr(), query));
}

void GameProvider::SetGameIndexForTest(GameIndex game_index) {
  game_index_ = std::move(game_index);
}

void GameProvider::OnSearchComplete(
    std::u16string query,
    std::vector<std::pair<const apps::Result*, double>> matches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sort matches by descending relevance score.
  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  SearchProvider::Results results;
  for (size_t i = 0; i < std::min(matches.size(), kMaxResults); ++i) {
    const apps::Result* result = matches[i].first;
    if (!result->GetSourceExtras() ||
        !result->GetSourceExtras()->AsGameExtras()) {
      // Result was not a game.
      continue;
    }
    results.emplace_back(std::make_unique<GameResult>(
        profile_, list_controller_, app_discovery_service_, *matches[i].first,
        matches[i].second, query));
  }
  LogStatus(Status::kOk);
  SwapResults(&results);
}

}  // namespace app_list
