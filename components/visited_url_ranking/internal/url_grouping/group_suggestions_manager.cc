// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace visited_url_ranking {

namespace {

FetchOptions GetFetchOptionsForSuggestions() {
  std::vector<URLVisitAggregatesTransformType> transforms{
      URLVisitAggregatesTransformType::kRecencyFilter,
      URLVisitAggregatesTransformType::kTabEventsData};

  const base::TimeDelta last_active_time_limit = base::Days(1);

  std::map<Fetcher, FetchOptions::FetchSources> fetcher_sources;
  fetcher_sources.emplace(
      Fetcher::kTabModel,
      FetchOptions::FetchSources({FetchOptions::Source::kLocal}));

  std::map<URLVisitAggregate::URLType, FetchOptions::ResultOption> result_map;
  result_map[URLVisitAggregate::URLType::kActiveLocalTab] =
      FetchOptions::ResultOption{.age_limit = last_active_time_limit};
  return FetchOptions(std::move(result_map), std::move(fetcher_sources),
                      base::Time::Now() - last_active_time_limit,
                      std::move(transforms));
}

}  // namespace

class GroupSuggestionsManager::GroupSuggestionComputer {
 public:
  GroupSuggestionComputer(VisitedURLRankingService* visited_url_ranking_service,
                          const GroupSuggestionsService::Scope& scope)
      : visited_url_ranking_service_(visited_url_ranking_service),
        suggestion_scope_(scope) {}

  ~GroupSuggestionComputer() = default;
  GroupSuggestionComputer(const GroupSuggestionComputer&) = delete;
  GroupSuggestionComputer& operator=(const GroupSuggestionComputer&) = delete;

  void Start(GroupingHeuristics::SuggestionsCallback callback) {
    visited_url_ranking_service_->FetchURLVisitAggregates(
        GetFetchOptionsForSuggestions(),
        base::BindOnce(&GroupSuggestionComputer::OnFetchedCandidates,
                       weak_ptr_factory.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnFetchedCandidates(GroupingHeuristics::SuggestionsCallback callback,
                           ResultStatus status,
                           URLVisitsMetadata metadata,
                           std::vector<URLVisitAggregate> candidates) {
    VLOG(1) << "GroupSuggestionComputer::OnFetchedCandidates: "
            << candidates.size();
    std::erase_if(candidates,
                  [&](auto& candidate) { return !ShouldInclude(candidate); });

    heuristics_.GetSuggestions(std::move(candidates), std::move(callback));
  }

  bool ShouldInclude(const URLVisitAggregate& candidate) {
    auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
    if (tab_data_it == candidate.fetcher_data_map.end()) {
      return false;
    }
    auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
    // Skip if already in group.
    if (tab->last_active_tab.tab_metadata.local_tab_group_id) {
      return false;
    }
    // TODO(ssid): Remove tabs that are not in `suggestion_scope_`.

    return true;
  }

  GroupingHeuristics heuristics_;
  const raw_ptr<VisitedURLRankingService> visited_url_ranking_service_;
  GroupSuggestionsService::Scope suggestion_scope_;
  GroupingHeuristics::SuggestionsCallback callback_;

  base::WeakPtrFactory<GroupSuggestionComputer> weak_ptr_factory{this};
};

GroupSuggestionsManager::GroupSuggestionsManager(
    VisitedURLRankingService* visited_url_ranking_service)
    : visited_url_ranking_service_(visited_url_ranking_service) {}

GroupSuggestionsManager::~GroupSuggestionsManager() = default;

void GroupSuggestionsManager::MaybeTriggerSuggestions(
    const GroupSuggestionsService::Scope& scope) {
  VLOG(1)
      << "GroupSuggestionsManager::MaybeTriggerSuggestions. Ongoing compute: "
      << !!suggestion_computer_;

  // Stop any ongoing computation since tab state has been updated.
  suggestion_computer_.reset();

  // TODO: maybe throttle the computations for efficiency.
  suggestion_computer_ = std::make_unique<GroupSuggestionComputer>(
      visited_url_ranking_service_, scope);
  suggestion_computer_->Start(
      base::BindOnce(&GroupSuggestionsManager::ShowSuggestion,
                     weak_ptr_factory_.GetWeakPtr(), scope));
}

void GroupSuggestionsManager::RegisterDelegate(
    GroupSuggestionsDelegate* delegate,
    const GroupSuggestionsService::Scope& scope) {
  if (registered_delegates_.count(delegate)) {
    CHECK(scope == registered_delegates_[delegate].scope);
    return;
  }
  registered_delegates_.emplace(
      delegate, DelegateMetadata{.delegate = delegate, .scope = scope});
}

void GroupSuggestionsManager::UnregisterDelegate(
    GroupSuggestionsDelegate* delegate) {
  registered_delegates_.erase(delegate);
}

bool GroupSuggestionsManager::GetCurrentComputationForTesting() const {
  return !!suggestion_computer_;
}

void GroupSuggestionsManager::ShowSuggestion(
    const GroupSuggestionsService::Scope& scope,
    std::optional<GroupSuggestions> suggestions) {
  if (!suggestions) {
    if (!suggestion_computed_callback_.is_null()) {
      suggestion_computed_callback_.Run();
    }
    return;
  }
  std::erase_if(suggestions->suggestions, [&](const auto& suggestion) {
    return suggestion_results_.contains(SuggestedTabs(suggestion.tab_ids));
  });
  if (suggestions->suggestions.empty()) {
    if (!suggestion_computed_callback_.is_null()) {
      suggestion_computed_callback_.Run();
    }
    return;
  }

  GroupSuggestionsDelegate* delegate = nullptr;
  for (auto it : registered_delegates_) {
    if (it.second.scope == scope) {
      delegate = it.second.delegate;
    }
  }
  if (delegate) {
    VLOG(1) << "Showing suggestion to group tabs "
            << suggestions->suggestions.size();
    std::vector<int> tab_ids = suggestions->suggestions[0].tab_ids;
    auto result_callback =
        base::BindOnce(&GroupSuggestionsManager::OnSuggestionResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(tab_ids));

    base::SequencedTaskRunner::GetCurrentDefault()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&GroupSuggestionsDelegate::ShowSuggestion,
                       base::Unretained(delegate), std::move(*suggestions),
                       std::move(result_callback)),
        suggestion_computed_callback_.is_null()
            ? base::DoNothing()
            : suggestion_computed_callback_);
  } else {
    VLOG(1) << "Suggestion discarded for " << scope.tab_session_id;
    if (!suggestion_computed_callback_.is_null()) {
      suggestion_computed_callback_.Run();
    }
  }
}

void GroupSuggestionsManager::OnSuggestionResult(
    const std::vector<int>& tab_ids,
    GroupSuggestionsDelegate::UserResponseMetadata user_response) {
  suggestion_results_[SuggestedTabs(tab_ids)] = std::move(user_response);
}

}  // namespace visited_url_ranking
