// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"

namespace visited_url_ranking {

GroupSuggestionsServiceImpl::GroupSuggestionsServiceImpl(
    VisitedURLRankingService* visited_url_ranking_service,
    TabEventsVisitTransformer* tab_events_transformer,
    PrefService* pref_service)
    : visited_url_ranking_service_(visited_url_ranking_service),
      tab_events_transformer_(tab_events_transformer),
      group_suggestions_manager_(
          std::make_unique<GroupSuggestionsManager>(visited_url_ranking_service,
                                                    pref_service)) {
  tab_tracker_ = std::make_unique<TabEventTrackerImpl>(
      base::BindRepeating(&GroupSuggestionsServiceImpl::OnNewSuggestionTabEvent,
                          weak_ptr_factory_.GetWeakPtr()));
  tab_events_transformer_->set_tab_event_tracker(tab_tracker_.get());
}

GroupSuggestionsServiceImpl::~GroupSuggestionsServiceImpl() {
  tab_events_transformer_->set_tab_event_tracker(nullptr);
  ClearAllUserData();
}

void GroupSuggestionsServiceImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  GroupSuggestionsTracker::RegisterProfilePrefs(registry);
}

TabEventTracker* GroupSuggestionsServiceImpl::GetTabEventTracker() {
  return tab_tracker_.get();
}

void GroupSuggestionsServiceImpl::RegisterDelegate(
    GroupSuggestionsDelegate* delegate,
    const Scope& scope) {
  group_suggestions_manager_->RegisterDelegate(delegate, scope);
}

void GroupSuggestionsServiceImpl::UnregisterDelegate(
    GroupSuggestionsDelegate* delegate) {
  group_suggestions_manager_->UnregisterDelegate(delegate);
}

void GroupSuggestionsServiceImpl::OnNewSuggestionTabEvent() {
  // TODO(ssid): Plumb in the scope from the trigger events.
  group_suggestions_manager_->MaybeTriggerSuggestions(
      GroupSuggestionsService::Scope());
}

void GroupSuggestionsServiceImpl::SetConfigForTesting(
    base::TimeDelta computation_delay) {
  group_suggestions_manager_->set_consecutive_computation_delay_for_testing(
      computation_delay);
}

}  // namespace visited_url_ranking
