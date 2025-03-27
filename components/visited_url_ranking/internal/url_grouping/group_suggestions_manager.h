// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_MANAGER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace visited_url_ranking {

// Tracks and runs computation of suggestions.
class GroupSuggestionsManager {
 public:
  explicit GroupSuggestionsManager(
      VisitedURLRankingService* visited_url_ranking_service);
  ~GroupSuggestionsManager();

  GroupSuggestionsManager(const GroupSuggestionsManager&) = delete;
  GroupSuggestionsManager& operator=(const GroupSuggestionsManager&) = delete;

  // Compute the suggestions based on latest events and tab state, called when
  // new events were observed to try looking for suggestions.
  void MaybeTriggerSuggestions(const GroupSuggestionsService::Scope& scope);

  // Register and unregister delegate, see SuggestionsDelegate.
  void RegisterDelegate(GroupSuggestionsDelegate* delegate,
                        const GroupSuggestionsService::Scope& scope);
  void UnregisterDelegate(GroupSuggestionsDelegate* delegate);

  bool GetCurrentComputationForTesting() const;

  void set_suggestion_computed_callback_for_testing(
      base::RepeatingClosure callback) {
    suggestion_computed_callback_ = std::move(callback);
  }

 private:
  class GroupSuggestionComputer;

  struct DelegateMetadata {
    raw_ptr<GroupSuggestionsDelegate> delegate;
    GroupSuggestionsService::Scope scope;
  };

  using SuggestedTabs = base::flat_set<int>;
  using SuggestionResults =
      std::map<SuggestedTabs, GroupSuggestionsDelegate::UserResponseMetadata>;

  void ShowSuggestion(const GroupSuggestionsService::Scope& scope,
                      std::optional<GroupSuggestions> suggestions);

  void OnSuggestionResult(
      const std::vector<int>& tab_ids,
      GroupSuggestionsDelegate::UserResponseMetadata user_response);

  const raw_ptr<VisitedURLRankingService> visited_url_ranking_service_;
  base::flat_map<GroupSuggestionsDelegate*, DelegateMetadata>
      registered_delegates_;

  base::RepeatingClosure suggestion_computed_callback_;

  std::unique_ptr<GroupSuggestionComputer> suggestion_computer_;
  SuggestionResults suggestion_results_;

  base::WeakPtrFactory<GroupSuggestionsManager> weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_MANAGER_H_
