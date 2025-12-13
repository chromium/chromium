// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"

#include <unordered_map>
#include <variant>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/url_visit_util.h"

namespace visited_url_ranking {
namespace {

using HeuristicResults =
    base::flat_map<GroupSuggestion::SuggestionReason, std::vector<float>>;
using segmentation_platform::processing::ProcessedValue;

// Min number of tabs for each heuristic type before suggesting.
constexpr auto kReasonToMinTabCount =
    base::MakeFixedFlatMap<GroupSuggestion::SuggestionReason, unsigned>({
        {GroupSuggestion::SuggestionReason::kRecentlyOpened, 4},
        {GroupSuggestion::SuggestionReason::kSwitchedBetween, 2},
        {GroupSuggestion::SuggestionReason::kSimilarSource, 3},
        {GroupSuggestion::SuggestionReason::kSameOrigin, 3},
    });

// Limit for tab age till which a tab is considered recent.
constexpr base::TimeDelta kRecencyTabTimeLimit = base::Seconds(600);
// Number of switches to the tab to group with the current tab.
constexpr int kMinSwitchesToGroup = 2;

// history_clusters::Config::content_visibility_threshold
constexpr float kVisibilityScoreThreshold = 0.7;

UrlGroupingSuggestionId::Generator g_id_generator;

// A heuristic that find the recently opened tabs and groups them.
class RecentlyOpenedHeuristic : public GroupingHeuristics::Heuristic {
 public:
  RecentlyOpenedHeuristic()
      : GroupingHeuristics::Heuristic(
            GroupSuggestion::SuggestionReason::kRecentlyOpened) {}
  ~RecentlyOpenedHeuristic() override = default;

  std::vector<float> Run(
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs) override {
    std::vector<float> result(inputs.size(), 0.0f);
    const char* time_since_active_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec);
    unsigned count = 0;
    for (unsigned i = 0; i < inputs.size(); ++i) {
      std::optional<ProcessedValue> duration_sec =
          inputs[i]->GetMetadataArgument(time_since_active_input);
      if (duration_sec &&
          duration_sec->float_val < kRecencyTabTimeLimit.InSecondsF()) {
        result[i] = 1;
        ++count;
      }
    }
    CHECK_EQ(result.size(), inputs.size());
    base::UmaHistogramCounts100(
        "GroupSuggestionsService.OpenedTabCount.Last10Mins", count);
    return result;
  }
};

// A heuristic that find the tabs switched to often and groups them.
class SwitchedBetweenHeuristic : public GroupingHeuristics::Heuristic {
 public:
  SwitchedBetweenHeuristic()
      : GroupingHeuristics::Heuristic(
            GroupSuggestion::SuggestionReason::kSwitchedBetween) {}
  ~SwitchedBetweenHeuristic() override = default;

  std::vector<float> Run(
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs) override {
    std::vector<float> result(inputs.size(), 0.0f);
    const char* tab_recent_foreground_count_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTabRecentForegroundCount);
    for (unsigned i = 0; i < inputs.size(); ++i) {
      std::optional<ProcessedValue> tab_recent_foreground_count =
          inputs[i]->GetMetadataArgument(tab_recent_foreground_count_input);
      if (tab_recent_foreground_count &&
          tab_recent_foreground_count->float_val >= kMinSwitchesToGroup) {
        result[i] = 1;
      }
    }
    return result;
  }
};

// A heuristic that find the tabs from the same source and groups them.
class SimilarSourceHeuristic : public GroupingHeuristics::Heuristic {
 public:
  SimilarSourceHeuristic()
      : GroupingHeuristics::Heuristic(
            GroupSuggestion::SuggestionReason::kSimilarSource) {}
  ~SimilarSourceHeuristic() override = default;

  std::vector<float> Run(
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs) override {
    std::vector<float> result(inputs.size(), 0.0f);
    const char* tab_opened_by_user_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kIsTabOpenedByUser);
    const char* tab_parent_id_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTabParentId);
    const char* tab_group_sync_id_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTabGroupSyncId);
    const char* tab_id_input =
        GetNameForInput(URLVisitAggregateRankingModelInputSignals::kTabId);
    const char* time_since_active_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec);

    std::unordered_map<float, float> tab_id_to_parent_id_map;
    tab_id_to_parent_id_map.reserve(inputs.size());
    std::unordered_map<float, unsigned> tab_id_to_tab_index_map;
    tab_id_to_tab_index_map.reserve(inputs.size());
    for (unsigned i = 0; i < inputs.size(); ++i) {
      std::optional<ProcessedValue> tab_opened_by_user =
          inputs[i]->GetMetadataArgument(tab_opened_by_user_input);
      std::optional<ProcessedValue> tab_parent_id =
          inputs[i]->GetMetadataArgument(tab_parent_id_input);
      std::optional<ProcessedValue> tab_group_sync_id =
          inputs[i]->GetMetadataArgument(tab_group_sync_id_input);
      std::optional<ProcessedValue> tab_id =
          inputs[i]->GetMetadataArgument(tab_id_input);
      std::optional<ProcessedValue> duration_sec =
          inputs[i]->GetMetadataArgument(time_since_active_input);

      if (!tab_parent_id || !tab_id) {
        continue;
      }
      // A root tab can have invalid parent or itself as parent.
      bool is_root_tab = tab_parent_id->float_val == -1 ||
                         tab_parent_id->float_val == tab_id->float_val;

      if (!tab_opened_by_user || tab_opened_by_user->float_val == 0) {
        // Child tabs that were opened automatically, should not be grouped.
        if (!is_root_tab) {
          continue;
        }
      }
      if (tab_group_sync_id && !tab_group_sync_id->str_val.empty()) {
        // Not group tabs already grouped.
        continue;
      }
      if (duration_sec &&
          duration_sec->float_val >= kRecencyTabTimeLimit.InSecondsF()) {
        // Not group tabs that are not recent.
        continue;
      }

      tab_id_to_parent_id_map[tab_id->float_val] = tab_parent_id->float_val;
      tab_id_to_tab_index_map[tab_id->float_val] = i;
    }
    // Cluster tabs based on parent tab relationship by finding disjoint sets in
    // the tab-parent DAG.
    // A bool to track whether there are any cluster merge happen in each round.
    bool merged = true;
    while (merged) {
      merged = false;
      std::unordered_map<float, float> new_tab_id_to_parent_id_map;
      new_tab_id_to_parent_id_map.reserve(tab_id_to_parent_id_map.size());
      for (const auto& pair : tab_id_to_parent_id_map) {
        float tab_id = pair.first;
        float parent_id = pair.second;
        if (base::Contains(tab_id_to_parent_id_map, parent_id) &&
            tab_id_to_parent_id_map[parent_id] != parent_id) {
          new_tab_id_to_parent_id_map[tab_id] =
              tab_id_to_parent_id_map[parent_id];
          // Keep track of merge.
          merged = true;
        } else {
          new_tab_id_to_parent_id_map[tab_id] = parent_id;
        }
      }
      tab_id_to_parent_id_map.swap(new_tab_id_to_parent_id_map);
    }
    for (const auto& pair : tab_id_to_tab_index_map) {
      // Add a high number to indicate that it is not tab ID, also to ensure
      // suggestion is valid when root tab id is 0. 0 is not valid cluster.
      result[pair.second] = tab_id_to_parent_id_map[pair.first] + 10000;
    }
    return result;
  }
};

// A heuristic that find the recently opened tabs of the same origin.
class SameOriginHeuristic : public GroupingHeuristics::Heuristic {
 public:
  SameOriginHeuristic()
      : GroupingHeuristics::Heuristic(
            GroupSuggestion::SuggestionReason::kSameOrigin) {}
  ~SameOriginHeuristic() override = default;

  std::vector<float> Run(
      const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
          inputs) override {
    std::vector<float> result(inputs.size(), 0.0f);
    const char* time_since_active_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec);
    const char* tab_url_hash = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTabUrlOriginHash);
    for (unsigned i = 0; i < inputs.size(); ++i) {
      std::optional<ProcessedValue> duration_sec =
          inputs[i]->GetMetadataArgument(time_since_active_input);
      std::optional<ProcessedValue> tab_url_origin_hash_value =
          inputs[i]->GetMetadataArgument(tab_url_hash);
      if (duration_sec &&
          duration_sec->float_val < kRecencyTabTimeLimit.InSecondsF() &&
          tab_url_origin_hash_value &&
          tab_url_origin_hash_value->float_val != 0) {
        result[i] = tab_url_origin_hash_value->float_val;
      }
    }
    CHECK_EQ(result.size(), inputs.size());
    return result;
  }
};

// Fills in the text to be shown to the user for the `suggestion`.
void SetSuggestionText(GroupSuggestion& suggestion) {
  // TODO(ssid): Set better messages and tab group names.
  switch (suggestion.suggestion_reason) {
    case GroupSuggestion::SuggestionReason::kUnknown:
      NOTREACHED();
    case GroupSuggestion::SuggestionReason::kSwitchedBetween:
      suggestion.promo_header = "Group recently selected tabs?";
      suggestion.promo_contents =
          "Switch between tabs easily with tab strip at the bottom.";
      suggestion.suggested_name = u"today";
      break;
    case GroupSuggestion::SuggestionReason::kSimilarSource:
      suggestion.promo_header = "Group recently opened tabs?";
      suggestion.promo_contents =
          "Organize recent tabs opened from the same tab.";
      suggestion.suggested_name = u"today";
      break;
    case GroupSuggestion::SuggestionReason::kRecentlyOpened:
      suggestion.promo_header = "Group recently opened tabs?";
      suggestion.promo_contents = "Organize recently opened tabs.";
      suggestion.suggested_name = u"today";
      break;
    case GroupSuggestion::SuggestionReason::kSameOrigin:
      suggestion.promo_header = "Group recently opened tabs?";
      suggestion.promo_contents =
          "Organize recently opened tabs from the same website.";
      suggestion.suggested_name = u"today";
      break;
  }
}

// Returns true if the group is visible.
bool IsGroupVisible(const GroupSuggestion& suggestion,
                    const std::vector<URLVisitAggregate>& candidates) {
  if (!features::kGroupSuggestionEnableVisibilityCheck.Get()) {
    return true;
  }
  std::map<int, bool> suggestion_tabs_visibility;
  for (const auto& candidate : candidates) {
    auto tab_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
    if (tab_it == candidate.fetcher_data_map.end()) {
      continue;
    }
    const auto& tab_data =
        std::get_if<URLVisitAggregate::TabData>(&tab_it->second);
    if (!tab_data) {
      continue;
    }

    int tab_id = tab_data->last_active_tab.id;
    if (!base::Contains(suggestion.tab_ids, tab_id)) {
      continue;
    }

    const auto& history_it = candidate.fetcher_data_map.find(Fetcher::kHistory);
    if (history_it != candidate.fetcher_data_map.end()) {
      const auto* history =
          std::get_if<URLVisitAggregate::HistoryData>(&history_it->second);
      if (history) {
        suggestion_tabs_visibility[tab_id] =
            history->last_visited.content_annotations.model_annotations
                .visibility_score > kVisibilityScoreThreshold;
      }
    }
  }

  // Return false if any tab in the suggestion does not have a score, or if any
  // tab is not visible.
  if (suggestion_tabs_visibility.size() != suggestion.tab_ids.size()) {
    return false;
  }
  for (const auto& [tab_id, is_visible] : suggestion_tabs_visibility) {
    if (!is_visible) {
      return false;
    }
  }
  return true;
}

std::optional<GroupSuggestion> GetSuggestionFromHeuristicResult(
    const std::vector<URLVisitAggregate>& candidates,
    GroupSuggestion::SuggestionReason reason,
    const std::vector<float>& outputs) {
  CHECK(!candidates.empty());
  GroupSuggestion suggestion;
  suggestion.suggestion_reason = reason;

  // Find the current tab based on the most recently active tab.
  int current_tab_index = -1;
  for (unsigned i = 0; i < candidates.size(); ++i) {
    auto it = candidates[i].fetcher_data_map.find(Fetcher::kTabModel);
    DCHECK(it != candidates[i].fetcher_data_map.end());
    if (it == candidates[i].fetcher_data_map.end()) {
      continue;
    }
    const auto& tab_data = std::get_if<URLVisitAggregate::TabData>(&it->second);
    DCHECK(tab_data);
    if (!tab_data) {
      continue;
    }
    if (tab_data->last_active_tab.tab_metadata.is_currently_active) {
      current_tab_index = i;
    }
  }
  if (current_tab_index == -1) {
    // If current tab is not a candidate (e.g. if it's a new tab page), don't
    // show.
    return std::nullopt;
  }
  float current_tab_cluster = outputs[current_tab_index];
  if (current_tab_cluster == 0) {
    // If current tab is not part of any cluster, don't show.
    return std::nullopt;
  }

  for (unsigned i = 0; i < outputs.size(); ++i) {
    if (outputs[i] != current_tab_cluster) {
      // If the candidate is not in current tab's cluster, skip from suggestion.
      continue;
    }
    auto it = candidates[i].fetcher_data_map.find(Fetcher::kTabModel);
    DCHECK(it != candidates[i].fetcher_data_map.end());
    if (it == candidates[i].fetcher_data_map.end()) {
      continue;
    }
    const auto& tab_data = std::get_if<URLVisitAggregate::TabData>(&it->second);
    DCHECK(tab_data);
    if (!tab_data) {
      continue;
    }
    suggestion.tab_ids.push_back(tab_data->last_active_tab.id);
  }
  // If the number of tabs per the heuristic is too low, dont show suggestion.
  unsigned min_tabs = kReasonToMinTabCount.at(reason);
  if (suggestion.tab_ids.size() < min_tabs) {
    return std::nullopt;
  }

  if (!IsGroupVisible(suggestion, candidates)) {
    VLOG(1) << "Suggestion discarded due to visibility";
    base::UmaHistogramEnumeration(
        "GroupSuggestionsService.SuggestionThrottledReason",
        TabGroupSuggestionThrottleReason::kGroupNotVisible);

    return std::nullopt;
  }

  suggestion.suggestion_id = g_id_generator.GenerateNextId();
  SetSuggestionText(suggestion);
  return suggestion;
}

std::optional<GroupSuggestions> GetAllGroupSuggestions(
    const std::vector<URLVisitAggregate>& candidates,
    const std::vector<GroupSuggestion::SuggestionReason>& heuristics_priority,
    const HeuristicResults& results) {
  GroupSuggestions suggestions;
  for (auto reason : heuristics_priority) {
    const auto& result = results.find(reason);
    if (result == results.end()) {
      continue;
    }
    auto suggestion = GetSuggestionFromHeuristicResult(
        candidates, result->first, result->second);
    if (!suggestion) {
      continue;
    }
    suggestions.suggestions.emplace_back(std::move(*suggestion));
  }
  if (suggestions.suggestions.empty()) {
    return std::nullopt;
  }
  return suggestions;
}

}  // namespace

GroupingHeuristics::SuggestionsResult::SuggestionsResult() = default;
GroupingHeuristics::SuggestionsResult::~SuggestionsResult() = default;
GroupingHeuristics::SuggestionsResult::SuggestionsResult(
    GroupingHeuristics::SuggestionsResult&&) = default;
GroupingHeuristics::SuggestionsResult&
GroupingHeuristics::SuggestionsResult::operator=(
    GroupingHeuristics::SuggestionsResult&& suggestion_result) = default;

GroupingHeuristics::GroupingHeuristics() {
  if (features::kGroupSuggestionEnableRecentlyOpened.Get()) {
    heuristics_.emplace(GroupSuggestion::SuggestionReason::kRecentlyOpened,
                        std::make_unique<RecentlyOpenedHeuristic>());
  }
  if (features::kGroupSuggestionEnableSwitchBetween.Get()) {
    heuristics_.emplace(GroupSuggestion::SuggestionReason::kSwitchedBetween,
                        std::make_unique<SwitchedBetweenHeuristic>());
  }
  if (features::kGroupSuggestionEnableSimilarSource.Get()) {
    heuristics_.emplace(GroupSuggestion::SuggestionReason::kSimilarSource,
                        std::make_unique<SimilarSourceHeuristic>());
  }
  if (features::kGroupSuggestionEnableSameOrigin.Get()) {
    heuristics_.emplace(GroupSuggestion::SuggestionReason::kSameOrigin,
                        std::make_unique<SameOriginHeuristic>());
  }
}

GroupingHeuristics::~GroupingHeuristics() = default;

void GroupingHeuristics::GetSuggestions(
    std::vector<URLVisitAggregate> candidates,
    GroupingHeuristics::SuggestionResultCallback callback) {
  GetSuggestions(std::move(candidates),
                 {GroupSuggestion::SuggestionReason::kSwitchedBetween,
                  GroupSuggestion::SuggestionReason::kSimilarSource,
                  GroupSuggestion::SuggestionReason::kSameOrigin,
                  GroupSuggestion::SuggestionReason::kRecentlyOpened},
                 std::move(callback));
}

void GroupingHeuristics::GetSuggestions(
    std::vector<URLVisitAggregate> candidates,
    const std::vector<GroupSuggestion::SuggestionReason>& heuristics_priority,
    SuggestionResultCallback callback) {
  SuggestionsResult result;
  if (candidates.empty()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  std::vector<scoped_refptr<segmentation_platform::InputContext>> signals;
  HeuristicResults heuristic_results;

  for (const auto& candidate : candidates) {
    signals.push_back(AsInputContext(kSuggestionsPredictionSchema, candidate));
  }
  for (const auto type : heuristics_priority) {
    if (!base::Contains(heuristics_, type)) {
      continue;
    }
    auto& heuristic = heuristics_[type];
    heuristic_results.emplace(heuristic->reason(), heuristic->Run(signals));
  }
  result.suggestions = GetAllGroupSuggestions(candidates, heuristics_priority,
                                              heuristic_results);
  result.inputs = signals;
  std::move(callback).Run(std::move(result));
}

}  // namespace visited_url_ranking
