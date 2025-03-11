// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
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
  GroupSuggestionComputer(UrlGroupingSuggestionId suggestion_id,
                          VisitedURLRankingService* visited_url_ranking_service,
                          const GroupSuggestionsService::Scope& scope)
      : suggestion_id_(suggestion_id),
        visited_url_ranking_service_(visited_url_ranking_service),
        suggestion_scope_(scope) {}

  ~GroupSuggestionComputer() = default;
  GroupSuggestionComputer(const GroupSuggestionComputer&) = delete;
  GroupSuggestionComputer& operator=(const GroupSuggestionComputer&) = delete;

  UrlGroupingSuggestionId suggestion_id() const { return suggestion_id_; }

  void Start() {
    visited_url_ranking_service_->FetchURLVisitAggregates(
        GetFetchOptionsForSuggestions(),
        base::BindOnce(&GroupSuggestionComputer::OnFetchedCandidates,
                       weak_ptr_factory.GetWeakPtr()));
  }

 private:
  void OnFetchedCandidates(ResultStatus,
                           URLVisitsMetadata,
                           std::vector<URLVisitAggregate>) {
    // TODO: run heuristcis on the data.
  }

  const UrlGroupingSuggestionId suggestion_id_;
  const raw_ptr<VisitedURLRankingService> visited_url_ranking_service_;
  GroupSuggestionsService::Scope suggestion_scope_;

  base::WeakPtrFactory<GroupSuggestionComputer> weak_ptr_factory{this};
};

GroupSuggestionsManager::GroupSuggestionsManager(
    VisitedURLRankingService* visited_url_ranking_service)
    : visited_url_ranking_service_(visited_url_ranking_service) {}

GroupSuggestionsManager::~GroupSuggestionsManager() = default;

UrlGroupingSuggestionId GroupSuggestionsManager::MaybeTriggerSuggestions(
    const GroupSuggestionsService::Scope& scope) {
  // Stop any ongoing computation since tab state has been updated.
  suggestion_computer_.reset();

  // TODO: maybe throttle the computations for efficiency.
  suggestion_computer_ = std::make_unique<GroupSuggestionComputer>(
      id_generator_.GenerateNextId(), visited_url_ranking_service_, scope);
  suggestion_computer_->Start();

  return suggestion_computer_->suggestion_id();
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

UrlGroupingSuggestionId
GroupSuggestionsManager::GetCurrentComputationForTesting() const {
  return suggestion_computer_ ? suggestion_computer_->suggestion_id()
                              : UrlGroupingSuggestionId();
}

}  // namespace visited_url_ranking
