// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace visited_url_ranking {

namespace {

using segmentation_platform::processing::ProcessedValue;

const base::FeatureParam<int> kConsecutiveComputationDelaySec{
    &features::kGroupSuggestionService, "consecutive_computation_delay_sec", 0};

FetchOptions GetFetchOptionsForSuggestions() {
  std::vector<URLVisitAggregatesTransformType> transforms{
      URLVisitAggregatesTransformType::kRecencyFilter,
      URLVisitAggregatesTransformType::kTabEventsData};

  const base::TimeDelta last_active_time_limit = base::Days(1);

  std::map<Fetcher, FetchOptions::FetchSources> fetcher_sources;
  fetcher_sources.emplace(
      Fetcher::kTabModel,
      FetchOptions::FetchSources({FetchOptions::Source::kLocal}));
  fetcher_sources.emplace(
      Fetcher::kHistory,
      FetchOptions::FetchSources({FetchOptions::Source::kLocal}));

  std::map<URLVisitAggregate::URLType, FetchOptions::ResultOption> result_map;
  result_map[URLVisitAggregate::URLType::kActiveLocalTab] =
      FetchOptions::ResultOption{.age_limit = last_active_time_limit};
  return FetchOptions(std::move(result_map), std::move(fetcher_sources),
                      base::Time::Now() - last_active_time_limit,
                      std::move(transforms));
}

void RecordSuggestionUKM(
    const GroupSuggestion& shown_suggestion,
    const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
        inputs,
    const GroupSuggestionsDelegate::UserResponseMetadata& user_response) {
  const char* tab_id_input =
      GetNameForInput(URLVisitAggregateRankingModelInputSignals::kTabId);
  const char* is_tab_selected_input = GetNameForInput(
      URLVisitAggregateRankingModelInputSignals::kIsTabSelected);
  const char* time_since_active_input = GetNameForInput(
      URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec);
  const char* tab_recent_foreground_count_input = GetNameForInput(
      URLVisitAggregateRankingModelInputSignals::kTabRecentForegroundCount);
  const char* ukm_source_id_input = GetNameForInput(
      URLVisitAggregateRankingModelInputSignals::kTabUkmSourceId);

  // Create an event id to tie the UKM rows together.
  uint64_t suggestion_event_id = base::RandUint64();

  // `inputs` is constructed as heuristics input and then carried over to here
  // for metrics collection. Should only get meta data from
  // kSuggestionsPredictionSchema in below.
  for (const auto& input : inputs) {
    std::optional<ProcessedValue> tab_id =
        input->GetMetadataArgument(tab_id_input);
    std::optional<ProcessedValue> is_tab_selected =
        input->GetMetadataArgument(is_tab_selected_input);
    std::optional<ProcessedValue> time_since_last_active =
        input->GetMetadataArgument(time_since_active_input);
    std::optional<ProcessedValue> foreground_count =
        input->GetMetadataArgument(tab_recent_foreground_count_input);
    std::optional<ProcessedValue> ukm_source_id =
        input->GetMetadataArgument(ukm_source_id_input);
    if (!base::Contains(shown_suggestion.tab_ids, tab_id->float_val) ||
        ukm_source_id->int64_val == ukm::kInvalidSourceId) {
      continue;
    }
    ukm::builders::GroupSuggestionPromo_Suggestion(ukm_source_id->int64_val)
        .SetIsActiveTab(is_tab_selected->float_val)
        .SetSecondsSinceLastActive(
            ukm::GetExponentialBucketMinForFineUserTiming(
                time_since_last_active->float_val))
        .SetRecentForegroundCount(foreground_count->float_val)
        .SetGroupSuggestionType(
            static_cast<int>(shown_suggestion.suggestion_reason))
        .SetUserResponseType(static_cast<int>(user_response.user_response))
        .SetGroupSuggestionID(suggestion_event_id)
        .Record(ukm::UkmRecorder::Get());
  }
}

void RecordTabIndexMetrics(
    const GroupSuggestion& shown_suggestion,
    const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
        inputs) {
  const char* tab_id_input =
      GetNameForInput(URLVisitAggregateRankingModelInputSignals::kTabId);
  const char* tab_index_input =
      GetNameForInput(URLVisitAggregateRankingModelInputSignals::kTabIndex);
  const char* is_last_tab_input =
      GetNameForInput(URLVisitAggregateRankingModelInputSignals::kIsLastTab);

  bool is_last_tab_in_suggestion = false;
  std::vector<int> tab_indexes;
  for (const auto& input : inputs) {
    std::optional<ProcessedValue> tab_id =
        input->GetMetadataArgument(tab_id_input);
    std::optional<ProcessedValue> tab_index =
        input->GetMetadataArgument(tab_index_input);
    std::optional<ProcessedValue> is_last_tab =
        input->GetMetadataArgument(is_last_tab_input);
    if (!base::Contains(shown_suggestion.tab_ids, tab_id->float_val)) {
      continue;
    }
    if (is_last_tab->float_val) {
      is_last_tab_in_suggestion = true;
    }
    if (tab_index->float_val != -1) {
      tab_indexes.push_back(tab_index->float_val);
    }
  }
  // Find the biggest and average index gap between every tab and its adjacent
  // tabs.
  std::sort(tab_indexes.begin(), tab_indexes.end());
  int max_gap = 0;
  int gap_sum = 0;
  for (size_t i = 1; i < tab_indexes.size(); ++i) {
    gap_sum += (tab_indexes[i] - tab_indexes[i - 1]);
    max_gap = std::max(max_gap, tab_indexes[i] - tab_indexes[i - 1]);
  }
  int average_gap =
      tab_indexes.size() > 1 ? gap_sum / (tab_indexes.size() - 1) : 0;

  base::UmaHistogramBoolean(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionContainsLastTab.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      is_last_tab_in_suggestion);
  base::UmaHistogramCounts100(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionTabIndexMaxGap.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      max_gap);
  base::UmaHistogramCounts100(
      base::StrCat(
          {"GroupSuggestionsService.TopSuggestionTabIndexAverageGap.",
           GetSuggestionReasonString(shown_suggestion.suggestion_reason)}),
      average_gap);
}
}  // namespace

class GroupSuggestionsManager::GroupSuggestionComputer {
 public:
  GroupSuggestionComputer(VisitedURLRankingService* visited_url_ranking_service,
                          const GroupSuggestionsService::Scope& scope)
      : visited_url_ranking_service_(visited_url_ranking_service),
        suggestion_scope_(scope) {}

  ~GroupSuggestionComputer() = default;
  GroupSuggestionComputer(const GroupSuggestionComputer&) = delete;
  GroupSuggestionComputer& operator=(const GroupSuggestionComputer&) = delete;

  void Start(GroupingHeuristics::SuggestionResultCallback callback) {
    visited_url_ranking_service_->FetchURLVisitAggregates(
        GetFetchOptionsForSuggestions(),
        base::BindOnce(&GroupSuggestionComputer::OnFetchedCandidates,
                       weak_ptr_factory.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnFetchedCandidates(
      GroupingHeuristics::SuggestionResultCallback callback,
      ResultStatus status,
      URLVisitsMetadata metadata,
      std::vector<URLVisitAggregate> candidates) {
    VLOG(1) << "GroupSuggestionComputer::OnFetchedCandidates: "
            << candidates.size();
    std::erase_if(candidates,
                  [&](auto& candidate) { return !ShouldInclude(candidate); });

    heuristics_.GetSuggestions(std::move(candidates), std::move(callback));
  }

  bool ShouldInclude(const URLVisitAggregate& candidate) {
    auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
    if (tab_data_it == candidate.fetcher_data_map.end()) {
      return false;
    }
    auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
    // Skip if already in group.
    if (tab->last_active_tab.tab_metadata.local_tab_group_id) {
      return false;
    }
    // TODO(ssid): Remove tabs that are not in `suggestion_scope_`.

    return true;
  }

  GroupingHeuristics heuristics_;
  const raw_ptr<VisitedURLRankingService> visited_url_ranking_service_;
  GroupSuggestionsService::Scope suggestion_scope_;

  base::WeakPtrFactory<GroupSuggestionComputer> weak_ptr_factory{this};
};

GroupSuggestionsManager::GroupSuggestionsManager(
    VisitedURLRankingService* visited_url_ranking_service,
    PrefService* pref_service)
    : visited_url_ranking_service_(visited_url_ranking_service),
      consecutive_computation_delay_(
          base::Seconds(kConsecutiveComputationDelaySec.Get())),
      suggestion_tracker_(
          std::make_unique<GroupSuggestionsTracker>(pref_service)) {}

GroupSuggestionsManager::~GroupSuggestionsManager() = default;

void GroupSuggestionsManager::MaybeTriggerSuggestions(
    const GroupSuggestionsService::Scope& scope) {
  // Skip computation if it ran recently to avoid overhead.
  if (!last_computation_time_.is_null() &&
      base::Time::Now() - last_computation_time_ <
          consecutive_computation_delay_) {
    return;
  }
  last_computation_time_ = base::Time::Now();

  VLOG(1)
      << "GroupSuggestionsManager::MaybeTriggerSuggestions. Ongoing compute: "
      << !!suggestion_computer_;

  // Stop any ongoing computation since tab state has been updated.
  suggestion_computer_.reset();

  // TODO: maybe throttle the computations for efficiency.
  suggestion_computer_ = std::make_unique<GroupSuggestionComputer>(
      visited_url_ranking_service_, scope);
  suggestion_computer_->Start(
      base::BindOnce(&GroupSuggestionsManager::OnFinishComputeSuggestions,
                     weak_ptr_factory_.GetWeakPtr(), scope));
}

void GroupSuggestionsManager::RegisterDelegate(
    GroupSuggestionsDelegate* delegate,
    const GroupSuggestionsService::Scope& scope) {
  if (registered_delegates_.count(delegate)) {
    CHECK(scope == registered_delegates_[delegate].scope);
    return;
  }
  registered_delegates_.emplace(
      delegate, DelegateMetadata{.delegate = delegate, .scope = scope});
}

void GroupSuggestionsManager::UnregisterDelegate(
    GroupSuggestionsDelegate* delegate) {
  registered_delegates_.erase(delegate);
}

void GroupSuggestionsManager::OnFinishComputeSuggestions(
    const GroupSuggestionsService::Scope& scope,
    GroupingHeuristics::SuggestionsResult result) {
  std::optional<GroupSuggestions> suggestions = std::move(result.suggestions);
  base::UmaHistogramCounts100(
      "GroupSuggestionsService.SuggestionsCount",
      suggestions ? suggestions->suggestions.size() : 0);
  if (!suggestions) {
    if (!suggestion_computed_callback_.is_null()) {
      suggestion_computed_callback_.Run();
    }
    return;
  }
  std::erase_if(suggestions->suggestions, [&](const auto& suggestion) {
    return !suggestion_tracker_->ShouldShowSuggestion(suggestion,
                                                      result.inputs);
  });
  base::UmaHistogramCounts100(
      "GroupSuggestionsService.SuggestionsCountAfterThrottling",
      suggestions->suggestions.size());

  if (suggestions->suggestions.empty()) {
    if (!suggestion_computed_callback_.is_null()) {
      suggestion_computed_callback_.Run();
    }
    return;
  }

  base::UmaHistogramEnumeration("GroupSuggestionsService.TopSuggestionReason",
                                suggestions->suggestions[0].suggestion_reason);
  base::UmaHistogramCounts100("GroupSuggestionsService.TopSuggestionTabCount",
                              suggestions->suggestions[0].tab_ids.size());
  base::UmaHistogramCounts100(
      base::StrCat({"GroupSuggestionsService.TopSuggestionTabCount.",
                    GetSuggestionReasonString(
                        suggestions->suggestions[0].suggestion_reason)}),
      suggestions->suggestions[0].tab_ids.size());

  RecordTabIndexMetrics(suggestions->suggestions[0], result.inputs);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GroupSuggestionsManager::ShowSuggestion,
                                weak_ptr_factory_.GetWeakPtr(), scope,
                                std::move(*suggestions), result.inputs));
}

void GroupSuggestionsManager::ShowSuggestion(
    const GroupSuggestionsService::Scope& scope,
    std::optional<GroupSuggestions> suggestions,
    const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
        inputs) {
  VLOG(1) << "Showing suggestion to group tabs "
          << suggestions->suggestions.size();

  for (auto it : registered_delegates_) {
    if (it.second.scope != scope) {
      continue;
    }
    GroupSuggestionsDelegate* delegate = it.second.delegate;
    auto result_callback =
        base::BindOnce(&GroupSuggestionsManager::OnSuggestionResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       suggestions->suggestions.front().DeepCopy(), inputs);

    delegate->ShowSuggestion(std::move(*suggestions),
                             std::move(result_callback));
  }
  if (!suggestion_computed_callback_.is_null()) {
    suggestion_computed_callback_.Run();
  }
}

void GroupSuggestionsManager::OnSuggestionResult(
    const GroupSuggestion& shown_suggestion,
    const std::vector<scoped_refptr<segmentation_platform::InputContext>>&
        inputs,
    GroupSuggestionsDelegate::UserResponseMetadata user_response) {
  RecordSuggestionUKM(shown_suggestion, inputs, user_response);
  if (user_response.user_response ==
          GroupSuggestionsDelegate::UserResponse::kNotShown ||
      user_response.user_response ==
          GroupSuggestionsDelegate::UserResponse::kUnknown) {
    return;
  }
  // TODO(ssid): Track all suggestions instead of assuming UI shows the first.
  DCHECK_EQ(user_response.suggestion_id, shown_suggestion.suggestion_id);
  suggestion_tracker_->AddSuggestion(shown_suggestion, inputs,
                                     user_response.user_response);
}

}  // namespace visited_url_ranking
