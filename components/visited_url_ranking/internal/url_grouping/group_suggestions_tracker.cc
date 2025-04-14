// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_tracker.h"

#include <unordered_set>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"

namespace visited_url_ranking {

namespace {

constexpr base::TimeDelta kSuggestionAgeLimit = base::Hours(24);

constexpr auto kReasonToMaxOverlappingTabs =
    base::MakeFixedFlatMap<GroupSuggestion::SuggestionReason, float>({
        {GroupSuggestion::SuggestionReason::kRecentlyOpened, 0.55},
        {GroupSuggestion::SuggestionReason::kSwitchedBetween, 0.60},
        {GroupSuggestion::SuggestionReason::kSimilarSource, 0.55},
        {GroupSuggestion::SuggestionReason::kSameOrigin, 0.55},
    });

float GetOverlappingTabCount(const base::flat_set<int>& shown_tabs,
                             const std::vector<int>& tab_ids) {
  std::vector<int> overlap;
  for (int x : tab_ids) {
    if (shown_tabs.count(x)) {
      overlap.push_back(x);
    }
  }
  return static_cast<float>(overlap.size()) / tab_ids.size();
}

}  // namespace

const char GroupSuggestionsTracker::kGroupSuggestionsTrackerStatePref[] =
    "group_suggestions_tracker.state";
const char GroupSuggestionsTracker::kGroupSuggestionsTrackerTimeKey[] =
    "time_shown";
const char GroupSuggestionsTracker::kGroupSuggestionsTrackerUserResponseKey[] =
    "user_response";
const char GroupSuggestionsTracker::kGroupSuggestionsTrackerUserTabIdsKey[] =
    "tab_ids";

GroupSuggestionsTracker::GroupSuggestionsTracker(PrefService* pref_service)
    : pref_service_(pref_service) {
  const auto& old_suggestion_list =
      pref_service_->GetList(kGroupSuggestionsTrackerStatePref);
  base::Time now = base::Time::Now();
  base::Value::List new_suggestion_list;
  for (const base::Value& old_suggestion : old_suggestion_list) {
    // Not including old suggestions in the new list.
    auto suggestion_optional =
        GroupSuggestionsTracker::ShownSuggestion::FromDict(
            old_suggestion.GetDict());
    if (suggestion_optional.has_value() &&
        (now - suggestion_optional->time_shown <= kSuggestionAgeLimit)) {
      suggestions_.push_back(std::move(suggestion_optional.value()));
      new_suggestion_list.Append(old_suggestion.Clone());
    }
  }
  pref_service_->SetList(kGroupSuggestionsTrackerStatePref,
                         std::move(new_suggestion_list));
}

GroupSuggestionsTracker::~GroupSuggestionsTracker() = default;

GroupSuggestionsTracker::ShownSuggestion::ShownSuggestion() = default;
GroupSuggestionsTracker::ShownSuggestion::~ShownSuggestion() = default;
GroupSuggestionsTracker::ShownSuggestion::ShownSuggestion(
    GroupSuggestionsTracker::ShownSuggestion&&) = default;
GroupSuggestionsTracker::ShownSuggestion&
GroupSuggestionsTracker::ShownSuggestion::operator=(
    GroupSuggestionsTracker::ShownSuggestion&& suggestion) = default;

base::Value::Dict GroupSuggestionsTracker::ShownSuggestion::ToDict() const {
  base::Value::Dict shown_suggestion_dict;
  shown_suggestion_dict.Set(kGroupSuggestionsTrackerTimeKey,
                            base::TimeToValue(time_shown));
  base::Value::List suggestion_tab_ids;
  for (int tab_id : tab_ids) {
    suggestion_tab_ids.Append(tab_id);
  }
  shown_suggestion_dict.Set(kGroupSuggestionsTrackerUserTabIdsKey,
                            std::move(suggestion_tab_ids));
  shown_suggestion_dict.Set(kGroupSuggestionsTrackerUserResponseKey,
                            static_cast<int>(user_response));
  return shown_suggestion_dict;
}

std::optional<GroupSuggestionsTracker::ShownSuggestion>
GroupSuggestionsTracker::ShownSuggestion::FromDict(
    const base::Value::Dict& dict) {
  ShownSuggestion suggestion;
  // Populate shown time.
  const base::Value* timestamp_ptr = dict.Find(kGroupSuggestionsTrackerTimeKey);
  if (!timestamp_ptr) {
    return std::nullopt;
  }
  std::optional<base::Time> timestamp = base::ValueToTime(*timestamp_ptr);
  if (!timestamp.has_value()) {
    return std::nullopt;
  }
  suggestion.time_shown = timestamp.value();

  // Populate tab ids.
  std::vector<int> tab_ids;
  auto* tab_ids_list_ptr = dict.FindList(kGroupSuggestionsTrackerUserTabIdsKey);
  if (!tab_ids_list_ptr) {
    return std::nullopt;
  }
  for (unsigned i = 0; i < tab_ids_list_ptr->size(); i++) {
    tab_ids.push_back((*tab_ids_list_ptr)[i].GetInt());
  }
  suggestion.tab_ids = std::move(tab_ids);

  // Populate user response.
  auto user_response_optional =
      dict.FindInt(kGroupSuggestionsTrackerUserResponseKey);
  if (!user_response_optional.has_value()) {
    return std::nullopt;
  }
  suggestion.user_response =
      static_cast<GroupSuggestionsDelegate::UserResponse>(
          user_response_optional.value());
  return suggestion;
}

void GroupSuggestionsTracker::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kGroupSuggestionsTrackerStatePref);
}

void GroupSuggestionsTracker::AddSuggestion(
    const GroupSuggestion& suggestion,
    GroupSuggestionsDelegate::UserResponse user_response) {
  ShownSuggestion item;
  item.time_shown = base::Time::Now();
  item.tab_ids = suggestion.tab_ids;
  item.user_response = user_response;
  suggestions_.push_back(std::move(item));

  // Append latest suggestion and remove old suggestions in storage.
  base::Time now = base::Time::Now();
  std::erase_if(suggestions_, [&](const ShownSuggestion& item) {
    return now - item.time_shown > kSuggestionAgeLimit;
  });
  base::Value::List new_suggestion_list;
  for (const ShownSuggestion& shown_suggestion : suggestions_) {
    new_suggestion_list.Append(shown_suggestion.ToDict());
  }
  pref_service_->SetList(kGroupSuggestionsTrackerStatePref,
                         std::move(new_suggestion_list));
}

bool GroupSuggestionsTracker::ShouldShowSuggestion(
    const GroupSuggestion& suggestion) {
  if (suggestion.tab_ids.empty() ||
      suggestion.suggestion_reason ==
          GroupSuggestion::SuggestionReason::kUnknown) {
    return false;
  }

  base::Time now = base::Time::Now();

  // Remove any old suggestions:
  std::erase_if(suggestions_, [&](const ShownSuggestion& item) {
    return now - item.time_shown > kSuggestionAgeLimit;
  });

  base::flat_set<int> all_shown;
  for (const auto& item : suggestions_) {
    all_shown.insert(item.tab_ids.begin(), item.tab_ids.end());
  }
  float overlap = GetOverlappingTabCount(all_shown, suggestion.tab_ids);
  if (overlap > kReasonToMaxOverlappingTabs.at(suggestion.suggestion_reason)) {
    return false;
  }
  return true;
}

}  // namespace visited_url_ranking
