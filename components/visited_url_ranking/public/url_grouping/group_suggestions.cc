// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"

namespace visited_url_ranking {

GroupSuggestion::GroupSuggestion() = default;
GroupSuggestion::~GroupSuggestion() = default;

GroupSuggestion::GroupSuggestion(GroupSuggestion&&) = default;
GroupSuggestion& GroupSuggestion::operator=(GroupSuggestion&&) = default;

GroupSuggestion GroupSuggestion::DeepCopy() const {
  GroupSuggestion copy;
  copy.tab_ids = tab_ids;
  copy.suggestion_reason = suggestion_reason;
  copy.suggested_name = suggested_name;
  copy.promo_header = promo_header;
  copy.promo_contents = promo_contents;
  copy.suggestion_id = suggestion_id;
  return copy;
}

GroupSuggestions::GroupSuggestions() = default;
GroupSuggestions::~GroupSuggestions() = default;

GroupSuggestions::GroupSuggestions(GroupSuggestions&&) = default;
GroupSuggestions& GroupSuggestions::operator=(GroupSuggestions&&) = default;

const char* GetSuggestionReasonString(
    GroupSuggestion::SuggestionReason reason) {
  switch (reason) {
    case GroupSuggestion::SuggestionReason::kUnknown:
      return "Unknown";
    case GroupSuggestion::SuggestionReason::kRecentlyOpened:
      return "RecentlyOpened";
    case GroupSuggestion::SuggestionReason::kSwitchedBetween:
      return "SwitchedBetween";
    case GroupSuggestion::SuggestionReason::kSimilarSource:
      return "SimilarSource";
    case GroupSuggestion::SuggestionReason::kSameOrigin:
      return "SameOrigin";
  }
  NOTREACHED();
}

}  // namespace visited_url_ranking
