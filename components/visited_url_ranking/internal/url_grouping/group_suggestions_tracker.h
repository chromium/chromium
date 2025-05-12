// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_

#include "base/containers/flat_set.h"
#include "components/segmentation_platform/public/input_context.h"
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
  static const char kGroupSuggestionsTrackerHostHashesKey[];

  explicit GroupSuggestionsTracker(PrefService* pref_service);
  ~GroupSuggestionsTracker();

  GroupSuggestionsTracker(const GroupSuggestionsTracker&) = delete;
  GroupSuggestionsTracker& operator=(const GroupSuggestionsTracker&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void AddSuggestion(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs,
      GroupSuggestionsDelegate::UserResponse user_response);

  bool ShouldShowSuggestion(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs);

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
    std::set<int> host_hashes;
    GroupSuggestionsDelegate::UserResponse user_response =
        GroupSuggestionsDelegate::UserResponse::kUnknown;
  };

  bool HasOverlappingTabs(const GroupSuggestion& suggestion) const;
  bool HasOverlappingHosts(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs) const;

  raw_ptr<PrefService> pref_service_;
  std::vector<ShownSuggestion> suggestions_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_
