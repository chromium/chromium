// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_provider.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/games/game_result.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

constexpr double kRelevanceThreshold = 0.7;
constexpr size_t kMaxResults = 3u;

double CalculateTitleRelevance(const TokenizedString& tokenized_query,
                               const std::u16string& game_title) {
  const TokenizedString tokenized_title(game_title,
                                        TokenizedString::Mode::kCamelCase);

  if (tokenized_query.text().empty() || tokenized_title.text().empty()) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  TokenizedStringMatch match;
  match.Calculate(tokenized_query, tokenized_title);
  return match.relevance();
}

std::vector<std::pair<GameData, double>> SearchGames(std::u16string query,
                                                     GameIndex* index) {
  TokenizedString tokenized_query(query, TokenizedString::Mode::kCamelCase);

  std::vector<std::pair<GameData, double>> matches;
  for (const auto& game_data : *index) {
    double relevance =
        CalculateTitleRelevance(tokenized_query, game_data.title);
    if (relevance > kRelevanceThreshold) {
      matches.push_back(std::make_pair(game_data, relevance));
    }
  }
  return matches;
}

}  // namespace

GameProvider::GameProvider(Profile* profile,
                           AppListControllerDelegate* list_controller)
    : profile_(profile), list_controller_(list_controller) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  game_index_manager_ = std::make_unique<GameIndexManager>();
  game_index_ = game_index_manager_->GetIndex();

  index_observer_.Observe(game_index_manager_.get());
}

GameProvider::~GameProvider() = default;

ash::AppListSearchResultType GameProvider::ResultType() const {
  return ash::AppListSearchResultType::kGames;
}

void GameProvider::OnIndexUpdated(const absl::optional<GameIndex>& index) {
  if (index)
    game_index_ = index;
}

void GameProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!game_index_)
    return;

  // Clear results and discard any existing searches.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&SearchGames, query, &game_index_.value()),
      base::BindOnce(&GameProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr(), query));
}

void GameProvider::SetGameIndexForTest(const GameIndex& game_index) {
  game_index_ = game_index;
}

void GameProvider::OnSearchComplete(
    std::u16string query,
    std::vector<std::pair<GameData, double>> matches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sort matches by descending relevance score.
  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  SearchProvider::Results results;
  for (size_t i = 0; i < std::min(matches.size(), kMaxResults); ++i) {
    results.emplace_back(std::make_unique<GameResult>(
        profile_, list_controller_, game_index_manager_.get(), matches[i].first,
        matches[i].second, query));
  }
  SwapResults(&results);
}

}  // namespace app_list
