// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"

#include <variant>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
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
    });

// Limit for tab age till which a tab is considered recent.
constexpr base::TimeDelta kRecencyTabTimeLimit = base::Seconds(600);
// Number of switches to the tab to group with the current tab.
constexpr int kMinSwitchesToGroup = 2;

UrlGroupingSuggestionId::Generator g_id_generator;

const char* GetNameForInput(URLVisitAggregateRankingModelInputSignals signal) {
  for (const auto& field : kSuggestionsPredictionSchema) {
    if (field.signal == signal) {
      return field.name;
    }
  }
  return nullptr;
}

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
    for (unsigned i = 0; i < inputs.size(); ++i) {
      std::optional<ProcessedValue> duration_sec =
          inputs[i]->GetMetadataArgument(time_since_active_input);
      if (duration_sec &&
          duration_sec->float_val < kRecencyTabTimeLimit.InSecondsF()) {
        result[i] = 1;
      }
    }
    CHECK_EQ(result.size(), inputs.size());
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
    const char* tab_launch_type_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kAndroidTabLaunchType);
    const char* tab_launch_package_name_input =
        GetNameForInput(URLVisitAggregateRankingModelInputSignals::
                            kAndroidTabLaunchPackageName);
    const char* tab_parent_id_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTabParentId);
    const char* tab_group_sync_id_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTabGroupSyncId);

    for (unsigned i = 0; i < inputs.size(); ++i) {
      std::optional<ProcessedValue> tab_opened_by_user =
          inputs[i]->GetMetadataArgument(tab_opened_by_user_input);
      std::optional<ProcessedValue> tab_launch_type =
          inputs[i]->GetMetadataArgument(tab_launch_type_input);
      std::optional<ProcessedValue> tab_launch_package_name =
          inputs[i]->GetMetadataArgument(tab_launch_package_name_input);
      std::optional<ProcessedValue> tab_parent_id =
          inputs[i]->GetMetadataArgument(tab_parent_id_input);
      std::optional<ProcessedValue> tab_group_sync_id =
          inputs[i]->GetMetadataArgument(tab_group_sync_id_input);

      if (!tab_opened_by_user || tab_opened_by_user->float_val == 0) {
        // Do not group tabs not opened by user.
        continue;
      }
      if (tab_group_sync_id && !tab_group_sync_id->str_val.empty()) {
        // Not group tabs already grouped.
        continue;
      }
      if (tab_launch_package_name &&
          !tab_launch_package_name->str_val.empty()) {
        // Assign a cluster ID based on hash of the package name.
        result[i] = base::FastHash(tab_launch_package_name->str_val);
        continue;
      }
      if (tab_parent_id) {
        result[i] = tab_parent_id->float_val;
        continue;
      }
      // TODO(ssid): Reconsider grouping based on launch types.
    }
    return result;
  }
};

// Fills in the text to be shown to the user for the `suggestion`.
void SetSuggestionText(GroupSuggestion& suggestion) {
  // TODO(ssid): Set better messages and tab group names.
  switch (suggestion.suggestion_reason) {
    case GroupSuggestion::SuggestionReason::kUnknown:
    case GroupSuggestion::SuggestionReason::kNumReasons:
      NOTREACHED();
    case GroupSuggestion::SuggestionReason::kSwitchedBetween:
      suggestion.promo_header = "Group tabs in bottom tab strip?";
      suggestion.promo_contents =
          "Switch between tabs easily with tab strip at the bottom.";
      suggestion.suggested_name = u"today";
      break;
    case GroupSuggestion::SuggestionReason::kSimilarSource:
      suggestion.promo_header = "Group recently opened tabs?";
      suggestion.promo_contents =
          "Organize recent tabs opened using the same action.";
      suggestion.suggested_name = u"today";
      break;
    case GroupSuggestion::SuggestionReason::kRecentlyOpened:
      suggestion.promo_header = "Group recently opened tabs?";
      suggestion.promo_contents = "Organize recently opened tabs.";
      suggestion.suggested_name = u"today";
      break;
  }
}

std::optional<GroupSuggestion> GetSuggestionFromHeuristicResult(
    const std::vector<URLVisitAggregate>& candidates,
    GroupSuggestion::SuggestionReason reason,
    const std::vector<float>& outputs) {
  CHECK(!candidates.empty());
  GroupSuggestion suggestion;
  suggestion.suggestion_reason = reason;

  // TODO(ssid): pass in current tab from tab fetcher.
  // Find the current tab based on the most recently active tab.
  int current_tab_index = -1;
  base::Time latest_active_tab;
  for (unsigned i = 0; i < candidates.size(); ++i) {
    base::Time last_visit_time = candidates[i].GetLastVisitTime();
    if (last_visit_time > latest_active_tab) {
      latest_active_tab = last_visit_time;
      current_tab_index = i;
    }
  }
  CHECK(current_tab_index != -1);  // At least one tab should exist.
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
    CHECK(it != candidates[i].fetcher_data_map.end());
    const auto& tab_data = std::get_if<URLVisitAggregate::TabData>(&it->second);
    CHECK(tab_data);
    suggestion.tab_ids.push_back(tab_data->last_active_tab.id);
  }
  // If the number of tabs per the heuristic is too low, dont show suggestion.
  unsigned min_tabs = kReasonToMinTabCount.at(reason);
  if (suggestion.tab_ids.size() < min_tabs) {
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
}

GroupingHeuristics::~GroupingHeuristics() = default;

void GroupingHeuristics::GetSuggestions(
    std::vector<URLVisitAggregate> candidates,
    GroupingHeuristics::SuggestionsCallback callback) {
  GetSuggestions(std::move(candidates),
                 {GroupSuggestion::SuggestionReason::kSwitchedBetween,
                  GroupSuggestion::SuggestionReason::kSimilarSource,
                  GroupSuggestion::SuggestionReason::kRecentlyOpened},
                 std::move(callback));
}

void GroupingHeuristics::GetSuggestions(
    std::vector<URLVisitAggregate> candidates,
    const std::vector<GroupSuggestion::SuggestionReason>& heuristics_priority,
    SuggestionsCallback callback) {
  if (candidates.empty()) {
    std::move(callback).Run(std::nullopt);
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

  std::move(callback).Run(GetAllGroupSuggestions(
      candidates, heuristics_priority, heuristic_results));
}

}  // namespace visited_url_ranking
