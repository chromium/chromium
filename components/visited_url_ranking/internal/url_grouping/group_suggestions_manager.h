// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_MANAGER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_tracker.h"
#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

class PrefService;

namespace visited_url_ranking {

// Tracks and runs computation of suggestions.
class GroupSuggestionsManager {
 public:
  GroupSuggestionsManager(VisitedURLRankingService* visited_url_ranking_service,
                          PrefService* pref_service);
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

  void set_suggestion_computed_callback_for_testing(
      base::RepeatingClosure callback) {
    suggestion_computed_callback_ = std::move(callback);
  }

  void set_consecutive_computation_delay_for_testing(base::TimeDelta delay) {
    consecutive_computation_delay_ = delay;
  }

 private:
  friend class GroupSuggestionsManagerTest;

  class GroupSuggestionComputer;

  struct DelegateMetadata {
    raw_ptr<GroupSuggestionsDelegate> delegate;
    GroupSuggestionsService::Scope scope;
  };

  void OnFinishComputeSuggestions(const GroupSuggestionsService::Scope& scope,
                                  GroupingHeuristics::SuggestionsResult result);

  void ShowSuggestion(
      const GroupSuggestionsService::Scope& scope,
      std::optional<GroupSuggestions> suggestions,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs);

  void OnSuggestionResult(
      const GroupSuggestion& shown_suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs,
      GroupSuggestionsDelegate::UserResponseMetadata user_response);

  const raw_ptr<VisitedURLRankingService> visited_url_ranking_service_;
  base::flat_map<GroupSuggestionsDelegate*, DelegateMetadata>
      registered_delegates_;

  base::TimeDelta consecutive_computation_delay_;

  base::RepeatingClosure suggestion_computed_callback_;

  std::unique_ptr<GroupSuggestionComputer> suggestion_computer_;
  std::unique_ptr<GroupSuggestionsTracker> suggestion_tracker_;

  base::Time last_computation_time_;

  base::WeakPtrFactory<GroupSuggestionsManager> weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_MANAGER_H_
