// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"

#include <variant>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/url_visit_util.h"

namespace visited_url_ranking {
namespace {

using HeuristicResults =
    base::flat_map<GroupSuggestion::SuggestionReason, std::vector<float>>;
UrlGroupingSuggestionId::Generator g_id_generator;

constexpr base::TimeDelta kRecencyTabTimeLimit = base::Seconds(600);

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
    const char* time_since_last_active_input = GetNameForInput(
        URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec);
    for (unsigned i = 0; i < inputs.size(); ++i) {
      // TODO(ssid): This should use creation time, not active time.
      std::optional<segmentation_platform::processing::ProcessedValue>
          duration_sec =
              inputs[i]->GetMetadataArgument(time_since_last_active_input);
      if (duration_sec &&
          duration_sec->float_val < kRecencyTabTimeLimit.InSecondsF()) {
        result[i] = 1;
      }
    }
    CHECK_EQ(result.size(), inputs.size());
    return result;
  }
};

// Fills in the text to be shown to the user for the `suggestion`.
void SetSuggestionText(GroupSuggestion::SuggestionReason reason,
                       GroupSuggestion& suggestion) {
  suggestion.suggestion_reason = reason;
  switch (reason) {
    case GroupSuggestion::SuggestionReason::kUnknown:
    case GroupSuggestion::SuggestionReason::kSwitchedBetween:
    case GroupSuggestion::SuggestionReason::kSimilarSource:
      NOTREACHED();
    case GroupSuggestion::SuggestionReason::kRecentlyOpened:
      suggestion.promo_header = "Group recently opened tabs?";
      suggestion.promo_contents = "Organize recently opened tabs";
      suggestion.suggested_name = u"today";  // update to include date and time
      break;
  }
}

std::optional<GroupSuggestion> GetSuggestionFromHeuristicResult(
    const std::vector<URLVisitAggregate>& candidates,
    const std::vector<float>& outputs) {
  GroupSuggestion suggestion;
  for (unsigned i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > 0) {
      auto it = candidates[i].fetcher_data_map.find(Fetcher::kTabModel);
      CHECK(it != candidates[i].fetcher_data_map.end());
      const auto& tab_data =
          std::get_if<URLVisitAggregate::TabData>(&it->second);
      CHECK(tab_data);
      suggestion.tab_ids.push_back(tab_data->last_active_tab.id);
    }
  }
  if (suggestion.tab_ids.size() <= 1) {
    return std::nullopt;
  }
  suggestion.suggestion_id = g_id_generator.GenerateNextId();
  return suggestion;
}

std::optional<GroupSuggestions> GetAllGroupSuggestions(
    const std::vector<URLVisitAggregate>& candidates,
    const HeuristicResults& results) {
  GroupSuggestions suggestions;
  for (const auto& result : results) {
    auto suggestion =
        GetSuggestionFromHeuristicResult(candidates, result.second);
    if (!suggestion) {
      continue;
    }
    SetSuggestionText(result.first, *suggestion);
    suggestions.suggestions.emplace_back(std::move(*suggestion));
  }
  if (suggestions.suggestions.empty()) {
    return std::nullopt;
  }
  return suggestions;
}

}  // namespace

GroupingHeuristics::GroupingHeuristics() {
  heuristics_.emplace(GroupSuggestion::SuggestionReason::kRecentlyOpened,
                      std::make_unique<RecentlyOpenedHeuristic>());
}

GroupingHeuristics::~GroupingHeuristics() = default;

void GroupingHeuristics::GetSuggestions(
    std::vector<URLVisitAggregate> candidates,
    GroupingHeuristics::SuggestionsCallback callback) {
  GetSuggestions(std::move(candidates),
                 {GroupSuggestion::SuggestionReason::kRecentlyOpened},
                 std::move(callback));
}

void GroupingHeuristics::GetSuggestions(
    std::vector<URLVisitAggregate> candidates,
    const std::vector<GroupSuggestion::SuggestionReason>& heuristics_to_run,
    SuggestionsCallback callback) {
  std::vector<scoped_refptr<segmentation_platform::InputContext>> signals;
  HeuristicResults heuristic_results;

  for (const auto& candidate : candidates) {
    signals.push_back(AsInputContext(kSuggestionsPredictionSchema, candidate));
  }
  for (const auto type : heuristics_to_run) {
    auto& heuristic = heuristics_[type];
    heuristic_results.emplace(heuristic->reason(), heuristic->Run(signals));
  }

  std::move(callback).Run(
      GetAllGroupSuggestions(candidates, heuristic_results));
}

}  // namespace visited_url_ranking
