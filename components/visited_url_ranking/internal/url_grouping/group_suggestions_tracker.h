// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_

#include "base/containers/flat_set.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"

class PrefService;
class PrefRegistrySimple;

namespace visited_url_ranking {

class GroupSuggestionsTracker {
 public:
  static const char kGroupSuggestionsTrackerStatePref[];
  static const char kGroupSuggestionsTrackerTimeKey[];
  static const char kGroupSuggestionsTrackerUserResponseKey[];
  static const char kGroupSuggestionsTrackerUserTabIdsKey[];

  explicit GroupSuggestionsTracker(PrefService* pref_service);
  ~GroupSuggestionsTracker();

  GroupSuggestionsTracker(const GroupSuggestionsTracker&) = delete;
  GroupSuggestionsTracker& operator=(const GroupSuggestionsTracker&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void AddSuggestion(const GroupSuggestion& suggestion,
                     GroupSuggestionsDelegate::UserResponse user_response);

  bool ShouldShowSuggestion(const GroupSuggestion& suggestion);

 private:
  struct ShownSuggestion {
    ShownSuggestion();
    ~ShownSuggestion();
    ShownSuggestion(ShownSuggestion&& suggestion);
    ShownSuggestion& operator=(ShownSuggestion&& suggestion);

    // Returns the dictionary representation of the `ShownSuggestion` to be
    // saved into pref.
    base::Value::Dict ToDict() const;

    // Returns ShownSuggestion extracted from a dictionary representation.
    static std::optional<ShownSuggestion> FromDict(
        const base::Value::Dict& dict);

    base::Time time_shown;
    std::vector<int> tab_ids;
    GroupSuggestionsDelegate::UserResponse user_response =
        GroupSuggestionsDelegate::UserResponse::kUnknown;
  };

  raw_ptr<PrefService> pref_service_;
  std::vector<ShownSuggestion> suggestions_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_
