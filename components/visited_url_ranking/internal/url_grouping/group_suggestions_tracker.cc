// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_tracker.h"

#include <unordered_set>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"

namespace visited_url_ranking {

namespace {

constexpr base::TimeDelta kSuggestionAgeLimit = base::Hours(24);

constexpr auto kReasonToMaxOverlappingTabs =
    base::MakeFixedFlatMap<GroupSuggestion::SuggestionReason, float>({
        {GroupSuggestion::SuggestionReason::kRecentlyOpened, 0.55},
        {GroupSuggestion::SuggestionReason::kSwitchedBetween, 0.60},
        {GroupSuggestion::SuggestionReason::kSimilarSource, 0.55},
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

GroupSuggestionsTracker::GroupSuggestionsTracker() = default;
GroupSuggestionsTracker::~GroupSuggestionsTracker() = default;

GroupSuggestionsTracker::ShownSuggestion::ShownSuggestion() = default;
GroupSuggestionsTracker::ShownSuggestion::~ShownSuggestion() = default;
GroupSuggestionsTracker::ShownSuggestion::ShownSuggestion(
    GroupSuggestionsTracker::ShownSuggestion&&) = default;
GroupSuggestionsTracker::ShownSuggestion&
GroupSuggestionsTracker::ShownSuggestion::operator=(
    GroupSuggestionsTracker::ShownSuggestion&& suggestion) = default;

void GroupSuggestionsTracker::AddSuggestion(
    const GroupSuggestion& suggestion,
    GroupSuggestionsDelegate::UserResponse user_response) {
  ShownSuggestion item;
  item.time_shown = base::Time::Now();
  item.tab_ids = suggestion.tab_ids;
  item.user_response = user_response;
  suggestions_.push_back(std::move(item));
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
