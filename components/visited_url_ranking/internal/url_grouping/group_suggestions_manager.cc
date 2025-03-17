// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
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
    heuristics_.GetSuggestions(std::move(candidates), std::move(callback));
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
    return;
  }
  GroupSuggestionsDelegate* delegate = nullptr;
  for (auto it : registered_delegates_) {
    if (it.second.scope == scope) {
      delegate = it.second.delegate;
    }
  }
  if (delegate) {
    delegate->ShowSuggestion(*suggestions, base::DoNothing());
  } else {
    VLOG(1) << "Suggestion discarded for " << scope.tab_session_id;
  }
}

}  // namespace visited_url_ranking
