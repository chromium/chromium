// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"

class PrefService;
class PrefRegistrySimple;

namespace visited_url_ranking {

using CachedSuggestionsAndInputs =
    std::pair<GroupSuggestions,
              std::vector<scoped_refptr<segmentation_platform::InputContext>>>;

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

  void AddShownSuggestion(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs,
      UserResponse user_response);

  bool ShouldShowSuggestion(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs);

  // Caches the provided suggestions. Overwrites any previously cached data.
  void CacheSuggestions(
      GroupSuggestions suggestions,
      std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs);

  // Retrieves a copy of the last cached suggestions. Does not clear the cache.
  std::optional<CachedSuggestionsAndInputs> GetCachedSuggestions();

  // Invalidates the cached suggestions.
  void InvalidateCache();

 private:
  struct CachedSuggestionsData {
    CachedSuggestionsData(
        GroupSuggestions suggestions,
        std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs);
    ~CachedSuggestionsData();
    CachedSuggestionsData(CachedSuggestionsData&&);
    CachedSuggestionsData& operator=(CachedSuggestionsData&&);

    GroupSuggestions suggestions;
    std::vector<scoped_refptr<segmentation_platform::InputContext>> inputs;
    base::Time cache_time;
  };

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
    UserResponse user_response = UserResponse::kUnknown;
  };

  bool HasOverlappingTabs(const GroupSuggestion& suggestion) const;
  bool HasOverlappingHosts(
      const GroupSuggestion& suggestion,
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs) const;

  raw_ptr<PrefService> pref_service_;
  std::vector<ShownSuggestion> suggestions_;

  // Holds the last set of computed suggestions and their input contexts for the
  // GetCachedSuggestions API.
  std::optional<CachedSuggestionsData> last_cached_suggestions_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_TRACKER_H_
