// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using base::Time;
using base::TimeDelta;
using file_manager::file_tasks::FileTasksObserver;

// Limits how frequently models are queried for ranking results.
constexpr TimeDelta kMinSecondsBetweenFetches = TimeDelta::FromSeconds(1);

// Limits how frequently results are logged, due to the possibility of multiple
// ranking events occurring for each user action.
constexpr TimeDelta kMinTimeBetweenLogs = TimeDelta::FromSeconds(2);

constexpr char kLogFileOpenType[] = "RecurrenceRanker.LogFileOpenType";

// Keep in sync with value in search_controller_factory.cc.
constexpr float kBoostOfApps = 8.0f;

// How many zero state results are shown in the results list.
constexpr int kDisplayedZeroStateResults = 5;

// For zero state, how many times a result can be shown before it is no longer
// considered 'fresh' and can therefore override another result.
constexpr int kMaxCacheCountForOverride = 3;

// All ranking item types that appear in the zero-state results list. The
// ordering of this list is important and should be changed with care: it has
// some effect on scores of results. In particular, if the ranker is entirely
// empty, it is warmed up by adding elements in this list with scores for later
// elements slightly higher than earlier elements.
constexpr RankingItemType kZeroStateTypes[] = {
    RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
    RankingItemType::kDriveQuickAccess};

// Represents each model used within the SearchResultRanker.
enum class Model { NONE, APPS, MIXED_TYPES };

// Returns the model relevant for predicting launches for results with the given
// |type|.
Model ModelForType(RankingItemType type) {
  switch (type) {
    case RankingItemType::kFile:
    case RankingItemType::kOmniboxGeneric:
    case RankingItemType::kOmniboxBookmark:
    case RankingItemType::kOmniboxDocument:
    case RankingItemType::kOmniboxHistory:
    case RankingItemType::kOmniboxSearch:
    case RankingItemType::kZeroStateFile:
    case RankingItemType::kDriveQuickAccess:
      return Model::MIXED_TYPES;
    // Currently we don't rank arc app shortcuts. If this changes, add a case
    // here.
    case RankingItemType::kApp:
      return Model::APPS;
    default:
      return Model::NONE;
  }
}

// Represents various open types of file open events. These values persist to
// logs. Entries should not be renumbered and numeric values should never
// be reused.
enum class FileOpenType {
  kUnknown = 0,
  kLaunch = 1,
  kOpen = 2,
  kSaveAs = 3,
  kDownload = 4,
  kMaxValue = kDownload,
};

FileOpenType GetTypeFromFileTaskNotifier(FileTasksObserver::OpenType type) {
  switch (type) {
    case FileTasksObserver::OpenType::kLaunch:
      return FileOpenType::kLaunch;
    case FileTasksObserver::OpenType::kOpen:
      return FileOpenType::kOpen;
    case FileTasksObserver::OpenType::kSaveAs:
      return FileOpenType::kSaveAs;
    case FileTasksObserver::OpenType::kDownload:
      return FileOpenType::kDownload;
    default:
      return FileOpenType::kUnknown;
  }
}

// Performs any per-type normalization required on a search result ID. This is
// meant to simplify the space of IDs in cases where they are too sparse.
std::string NormalizeId(const std::string& id, RankingItemType type) {
  // Put any further normalizations here.
  switch (type) {
    case RankingItemType::kOmniboxGeneric:
    case RankingItemType::kOmniboxBookmark:
    case RankingItemType::kOmniboxDocument:
    case RankingItemType::kOmniboxHistory:
    case RankingItemType::kOmniboxSearch:
      // Heuristically check if the URL points to a Drive file. If so, strip
      // some extra information from it.
      if (GURL(id).host() == "docs.google.com")
        return SimplifyGoogleDocsUrlId(id);
      else
        return SimplifyUrlId(id);
    case RankingItemType::kApp:
      return NormalizeAppId(id);
    default:
      return id;
  }
}

// Linearly maps |score| to the range [min, max].
// |score| is assumed to be within [0.0, 1.0]; if it's greater than 1.0
// then max is returned; if it's less than 0.0, then min is returned.
float ReRange(const float score, const float min, const float max) {
  if (score >= 1.0f)
    return max;
  if (score <= 0.0f)
    return min;

  return min + score * (max - min);
}

}  // namespace

SearchResultRanker::SearchResultRanker(Profile* profile,
                                       history::HistoryService* history_service)
    : history_service_observer_(this), profile_(profile), weak_factory_(this) {
  DCHECK(profile);
  DCHECK(history_service);
  history_service_observer_.Add(history_service);
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    notifier->AddObserver(this);
  }
}

SearchResultRanker::~SearchResultRanker() {
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    notifier->RemoveObserver(this);
  }
}

void SearchResultRanker::InitializeRankers(
    SearchController* search_controller) {
  if (app_list_features::IsQueryBasedMixedTypesRankerEnabled()) {
    results_list_boost_coefficient_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableQueryBasedMixedTypesRanker,
        "boost_coefficient", 0.1);

    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(0u);
    config.set_condition_decay(0.5f);
    config.set_target_limit(base::GetFieldTrialParamByFeatureAsInt(
        app_list_features::kEnableQueryBasedMixedTypesRanker, "target_limit",
        200));
    config.set_target_decay(base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableQueryBasedMixedTypesRanker, "target_decay",
        0.8f));
    config.mutable_predictor()->mutable_default_predictor();

    if (GetFieldTrialParamByFeatureAsBool(
            app_list_features::kEnableQueryBasedMixedTypesRanker,
            "use_category_model", false)) {
      // Group ranker model.
      results_list_group_ranker_ = std::make_unique<RecurrenceRanker>(
          "QueryBasedMixedTypesGroup",
          profile_->GetPath().AppendASCII("results_list_group_ranker.pb"),
          config, chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
    } else if (GetFieldTrialParamByFeatureAsBool(
                   app_list_features::kEnableQueryBasedMixedTypesRanker,
                   "use_aggregated_model", false) ||
               app_list_features::IsAggregatedMlSearchRankingEnabled()) {
      use_aggregated_search_ranking_inference_ = true;
    } else {
      // Item ranker model.
      const std::string config_json = GetFieldTrialParamValueByFeature(
          app_list_features::kEnableQueryBasedMixedTypesRanker, "config");
      query_mixed_config_converter_ = JsonConfigConverter::Convert(
          config_json, "QueryBasedMixedTypes",
          base::BindOnce(
              [](SearchResultRanker* ranker,
                 const RecurrenceRankerConfigProto& default_config,
                 base::Optional<RecurrenceRankerConfigProto> parsed_config) {
                ranker->query_mixed_config_converter_.reset();
                if (ranker->json_config_parsed_for_testing_)
                  std::move(ranker->json_config_parsed_for_testing_).Run();
                ranker->query_based_mixed_types_ranker_ =
                    std::make_unique<RecurrenceRanker>(
                        "QueryBasedMixedTypes",
                        ranker->profile_->GetPath().AppendASCII(
                            "query_based_mixed_types_ranker.pb"),
                        parsed_config ? parsed_config.value() : default_config,
                        chromeos::ProfileHelper::IsEphemeralUserProfile(
                            ranker->profile_));
              },
              base::Unretained(this), config));
    }
  }

  if (app_list_features::IsZeroStateMixedTypesRankerEnabled()) {
    zero_state_item_coeff_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "item_coeff",
        zero_state_item_coeff_);
    zero_state_group_coeff_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "group_coeff",
        zero_state_group_coeff_);
    zero_state_paired_coeff_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "paired_coeff",
        zero_state_paired_coeff_);

    RecurrenceRankerConfigProto default_config;
    default_config.set_min_seconds_between_saves(240u);
    default_config.set_condition_limit(1u);
    default_config.set_condition_decay(0.9f);
    default_config.set_target_limit(5u);
    default_config.set_target_decay(0.9f);
    default_config.mutable_predictor()->mutable_default_predictor();
    auto* predictor =
        default_config.mutable_predictor()->mutable_frecency_predictor();
    predictor->set_decay_coeff(0.9f);

    const std::string config_json = GetFieldTrialParamValueByFeature(
        app_list_features::kEnableZeroStateMixedTypesRanker, "config");

    zero_state_config_converter_ = JsonConfigConverter::Convert(
        config_json, "ZeroStateGroups",
        base::BindOnce(
            [](SearchResultRanker* ranker,
               const RecurrenceRankerConfigProto& default_config,
               base::Optional<RecurrenceRankerConfigProto> parsed_config) {
              ranker->zero_state_config_converter_.reset();
              if (ranker->json_config_parsed_for_testing_)
                std::move(ranker->json_config_parsed_for_testing_).Run();
              ranker->zero_state_group_ranker_ =
                  std::make_unique<RecurrenceRanker>(
                      "ZeroStateGroups",
                      ranker->profile_->GetPath().AppendASCII(
                          "zero_state_group_ranker.pb"),
                      parsed_config ? parsed_config.value() : default_config,
                      chromeos::ProfileHelper::IsEphemeralUserProfile(
                          ranker->profile_));
            },
            base::Unretained(this), default_config));
  }

  search_ranking_event_logger_ =
      std::make_unique<SearchRankingEventLogger>(profile_, search_controller);

  app_launch_event_logger_ = std::make_unique<app_list::AppLaunchEventLogger>();

  bool apps_enabled = app_list_features::IsAppRankerEnabled();
  if (app_list_features::IsAggregatedMlAppRankingEnabled() ||
      (apps_enabled &&
       GetFieldTrialParamByFeatureAsBool(app_list_features::kEnableAppRanker,
                                         "use_topcat_ranker", false))) {
    using_aggregated_app_inference_ = true;
    app_launch_event_logger_->CreateRankings();
  } else if (apps_enabled) {
    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(1u);
    config.set_condition_decay(0.5);
    config.set_target_limit(200);
    config.set_target_decay(0.8);
    config.mutable_predictor()->mutable_default_predictor();

    app_ranker_ = std::make_unique<RecurrenceRanker>(
        "AppRanker", profile_->GetPath().AppendASCII("app_ranker.pb"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
  }
}

void SearchResultRanker::FetchRankings(const base::string16& query) {
  // The search controller potentially calls SearchController::FetchResults
  // several times for each user's search, so we cache the results of querying
  // the models for a short time, to prevent uneccessary queries.
  const auto& now = Time::Now();
  if (now - time_of_last_fetch_ < kMinSecondsBetweenFetches)
    return;
  time_of_last_fetch_ = now;
  last_query_ = query;

  if (query.empty() && zero_state_group_ranker_) {
    zero_state_group_ranks_.clear();
    zero_state_group_ranks_ = zero_state_group_ranker_->Rank();

    // If the zero state model is empty, warm it up by training once on each
    // group. The order if |kZeroStateTypes| means that DriveQuickAccess will
    // have a slightly higher score than ZeroStateFile, which will have a
    // slightly higher score than Omnibox.
    if (zero_state_group_ranks_.empty()) {
      for (const auto type : kZeroStateTypes) {
        zero_state_group_ranker_->Record(
            base::NumberToString(static_cast<int>(type)));
      }
    }
    zero_state_group_ranks_ = zero_state_group_ranker_->Rank();
  }

  if (results_list_group_ranker_) {
    group_ranks_.clear();
    group_ranks_ = results_list_group_ranker_->Rank();
  } else if (query_based_mixed_types_ranker_) {
    query_mixed_ranks_.clear();
    query_mixed_ranks_ =
        query_based_mixed_types_ranker_->Rank(base::UTF16ToUTF8(query));
  }

  if (app_ranker_)
    app_ranks_ = app_ranker_->Rank();
}

void SearchResultRanker::Rank(Mixer::SortedResults* results) {
  if (!results)
    return;

  std::sort(results->begin(), results->end(),
            [](const Mixer::SortData& a, const Mixer::SortData& b) {
              return a.score > b.score;
            });
  std::map<std::string, float> search_ranker_score_map;
  if (!last_query_.empty() && use_aggregated_search_ranking_inference_) {
    search_ranking_event_logger_->CreateRankings(results, last_query_.size());
    search_ranker_score_map = search_ranking_event_logger_->RetrieveRankings();
  }

  std::map<std::string, float> ranking_map;
  if (using_aggregated_app_inference_)
    ranking_map = app_launch_event_logger_->RetrieveRankings();
  int aggregated_app_rank_success_count = 0;
  int aggregated_app_rank_fail_count = 0;

  // Counts how many times a given type has been seen so far in the results
  // list.
  base::flat_map<RankingItemType, int> zero_state_type_counts;
  for (auto& result : *results) {
    const auto& type = RankingItemTypeFromSearchResult(*result.result);
    const auto& model = ModelForType(type);

    if (model == Model::MIXED_TYPES) {
      if (last_query_.empty() && zero_state_group_ranker_) {
        LogZeroStateResultScore(type, result.score);
        ScoreZeroStateItem(&result, type, &zero_state_type_counts);
      } else if (results_list_group_ranker_) {
        const auto& rank_it =
            group_ranks_.find(base::NumberToString(static_cast<int>(type)));
        // The ranker only contains entries trained with types relating to files
        // or the omnibox. This means scores for apps, app shortcuts, and answer
        // cards will be unchanged.
        if (rank_it != group_ranks_.end()) {
          // Ranker scores are guaranteed to be in [0,1]. But, enforce that the
          // result of tweaking does not put the score above 3.0, as that may
          // interfere with apps or answer cards.
          result.score = std::min(
              result.score + rank_it->second * results_list_boost_coefficient_,
              3.0);
        }
      } else if (query_based_mixed_types_ranker_) {
        const auto& rank_it =
            query_mixed_ranks_.find(NormalizeId(result.result->id(), type));
        if (rank_it != query_mixed_ranks_.end()) {
          result.score = std::min(
              result.score + rank_it->second * results_list_boost_coefficient_,
              3.0);
        }
      } else if (!last_query_.empty() &&
                 use_aggregated_search_ranking_inference_) {
        result.score = search_ranker_score_map[result.result->id()];
      }
    } else if (model == Model::APPS) {
      if (using_aggregated_app_inference_) {
        const std::string id = NormalizeAppId(result.result->id());

        const auto& ranked = ranking_map.find(id);
        if (ranked != ranking_map.end()) {
          result.score =
              kBoostOfApps + ReRange((ranked->second + 4.0) / 8.0, 0.67, 1.0);
          ++aggregated_app_rank_success_count;
        } else {
          ++aggregated_app_rank_fail_count;
        }
      } else {
        if (app_ranker_) {
          const auto& it = app_ranks_.find(NormalizeAppId(result.result->id()));
          if (it != app_ranks_.end()) {
            result.score = kBoostOfApps + ReRange(it->second, 0.67, 1.0);
          }
        }
      }
    }
  }
  if (using_aggregated_app_inference_ &&
      (aggregated_app_rank_success_count != 0 ||
       aggregated_app_rank_fail_count != 0)) {
    UMA_HISTOGRAM_COUNTS_1000("Apps.AppList.AggregatedMlAppRankSuccess",
                              aggregated_app_rank_success_count);
    UMA_HISTOGRAM_COUNTS_1000("Apps.AppList.AggregatedMlAppRankFail",
                              aggregated_app_rank_fail_count);
  }
}

void SearchResultRanker::ScoreZeroStateItem(
    Mixer::SortData* result,
    RankingItemType type,
    base::flat_map<RankingItemType, int>* type_counts) const {
  DCHECK(type == RankingItemType::kOmniboxGeneric ||
         type == RankingItemType::kZeroStateFile ||
         type == RankingItemType::kDriveQuickAccess);

  const float item_score =
      1.0f -
      ((*type_counts)[type] / static_cast<float>(kDisplayedZeroStateResults));

  const auto& rank_it = zero_state_group_ranks_.find(
      base::NumberToString(static_cast<int>(type)));

  const float group_score =
      (rank_it != zero_state_group_ranks_.end()) ? rank_it->second : 0.0f;
  const float score = zero_state_item_coeff_ * item_score +
                      zero_state_group_coeff_ * group_score +
                      zero_state_paired_coeff_ * item_score * group_score;
  result->score = score / (zero_state_item_coeff_ + zero_state_group_coeff_ +
                           zero_state_paired_coeff_);

  ++(*type_counts)[type];
}

void SearchResultRanker::Train(const AppLaunchData& app_launch_data) {
  if (app_launch_data.launched_from ==
          ash::AppListLaunchedFrom::kLaunchedFromGrid &&
      app_launch_event_logger_) {
    // Log the AppResult from the grid to the UKM system.
    app_launch_event_logger_->OnGridClicked(app_launch_data.id);
  } else if (app_launch_data.launch_type ==
                 ash::AppListLaunchType::kAppSearchResult &&
             app_launch_event_logger_) {
    // Log the AppResult (either in the search result page, or in chip form in
    // AppsGridView) to the UKM system.
    app_launch_event_logger_->OnSuggestionChipOrSearchBoxClicked(
        app_launch_data.id, app_launch_data.suggestion_index,
        static_cast<int>(app_launch_data.launched_from));
  }

  // Update rankings after logging the click.
  if (using_aggregated_app_inference_)
    app_launch_event_logger_->CreateRankings();

  auto model = ModelForType(app_launch_data.ranking_item_type);
  if (model == Model::MIXED_TYPES) {
    if (app_launch_data.query.empty() && zero_state_group_ranker_) {
      zero_state_group_ranker_->Record(base::NumberToString(
          static_cast<int>(app_launch_data.ranking_item_type)));
    } else if (results_list_group_ranker_) {
      results_list_group_ranker_->Record(base::NumberToString(
          static_cast<int>(app_launch_data.ranking_item_type)));
    } else if (query_based_mixed_types_ranker_) {
      query_based_mixed_types_ranker_->Record(
          NormalizeId(app_launch_data.id, app_launch_data.ranking_item_type),
          app_launch_data.query);
    }
  } else if (model == Model::APPS && app_ranker_) {
    app_ranker_->Record(NormalizeAppId(app_launch_data.id));
  }

  if (model == Model::MIXED_TYPES && app_launch_data.query.empty()) {
    LogZeroStateLaunchType(app_launch_data.ranking_item_type);

    if (zero_state_group_ranker_) {
      std::vector<std::string> weights;
      for (const auto& pair : *zero_state_group_ranker_->GetTargetData())
        weights.push_back(base::StrCat(
            {pair.first, ":", base::NumberToString(pair.second.last_score)}));
      VLOG(1) << "Zero state files model weights: ["
              << base::JoinString(weights, ", ") << "]";
    }
  }
}

void SearchResultRanker::LogSearchResults(
    const base::string16& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int launched_index) {
  search_ranking_event_logger_->Log(trimmed_query, results, launched_index);
}

void SearchResultRanker::ZeroStateResultsDisplayed(
    const ash::SearchResultIdWithPositionIndices& results) {
  for (auto& cache_item : zero_state_results_cache_) {
    for (const auto& result : results) {
      if (cache_item.second.first == result.id) {
        ++cache_item.second.second;
        break;
      }
    }
  }
}

void SearchResultRanker::OverrideZeroStateResults(
    Mixer::SortedResults* const results) {
  // Construct a list of pointers filtered to contain all zero state results
  // and sorted in decreasing order of score.
  std::vector<Mixer::SortData*> result_ptrs;
  for (auto& result : *results) {
    if (ModelForType(RankingItemTypeFromSearchResult(*result.result)) ==
        Model::MIXED_TYPES) {
      result_ptrs.emplace_back(&result);
    }
  }
  std::sort(result_ptrs.begin(), result_ptrs.end(),
            [](const Mixer::SortData* const a, const Mixer::SortData* const b) {
              return a->score > b->score;
            });

  // For each group, override a candidate in the displayed results if
  // necessary. The ordering of the groups in the foreach means that if, there
  // are multiple overrides necessary, a QuickAccess result is above a
  // ZeroStateFile result is above an Omnibox result.
  int next_modifiable_index = kDisplayedZeroStateResults - 1;
  for (auto group : kZeroStateTypes) {
    // Find the best result for the group.
    int candidate_override_index = -1;
    for (int i = 0; i < static_cast<int>(result_ptrs.size()); ++i) {
      const auto& result = *result_ptrs[i];
      if (RankingItemTypeFromSearchResult(*result.result) != group)
        continue;

      // This is the highest scoring result of this group. Reset the top-results
      // cache if needed.
      const auto& cache_it = zero_state_results_cache_.find(group);
      if (cache_it == zero_state_results_cache_.end() ||
          cache_it->second.first != result.result->id()) {
        zero_state_results_cache_[group] = {result.result->id(), 0};
      }
      const auto& cache_count = zero_state_results_cache_[group].second;

      if (i >= kDisplayedZeroStateResults &&
          cache_count < kMaxCacheCountForOverride) {
        candidate_override_index = i;
      }
      break;
    }

    // If a result from this group will already be displayed or there are no
    // results in the group, we have nothing to do.
    if (candidate_override_index == -1)
      continue;

    // TODO(crbug.com/1011221): Remove once the bug re. zero-state drive files
    // not being shown is resolved.
    VLOG(1) << "Zero state files override: newtype=" << static_cast<int>(group)
            << " newpos=" << next_modifiable_index;
    // Override the result at |next_modifiable_index| with
    // |candidate_override_index| by swapping their scores.
    std::swap(result_ptrs[candidate_override_index]->score,
              result_ptrs[next_modifiable_index]->score);
    --next_modifiable_index;
  }

  // TODO(crbug.com/1011221): Remove once the bug re. zero-state drive files not
  // being shown is resolved.
  VLOG(1) << "Zero state files setting result scores";
  for (const auto* result : result_ptrs) {
    VLOG(1) << "Zero state files result score: type="
            << static_cast<int>(
                   RankingItemTypeFromSearchResult(*(result->result)))
            << " score=" << result->score;
  }
}

void SearchResultRanker::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  // Log the open type of file open events
  for (const auto& file_open : file_opens) {
    UMA_HISTOGRAM_ENUMERATION(kLogFileOpenType,
                              GetTypeFromFileTaskNotifier(file_open.open_type));
    // TODO(chareszhao): move this outside of SearchResultRanker.
    CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
        {base::StrCat({"FileOpened-", file_open.path.value()})},
        {{"open_type", static_cast<int>(file_open.open_type)}});
  }
}

void SearchResultRanker::OnURLVisited(history::HistoryService* history_service,
                                      ui::PageTransition transition,
                                      const history::URLRow& row,
                                      const history::RedirectList& redirects,
                                      base::Time visit_time) {
  // TODO(chareszhao): move this outside of SearchResultRanker.
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat({"URLVisited-", row.url().spec()})},
      {{"PageTransition", static_cast<int>(transition)}});
}

void SearchResultRanker::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!query_based_mixed_types_ranker_)
    return;

  if (deletion_info.IsAllHistory()) {
    // TODO(931149): We clear the whole model because we expect most targets to
    // be URLs. In future, consider parsing the targets and only deleting URLs.
    query_based_mixed_types_ranker_->GetTargetData()->clear();
  } else {
    for (const auto& row : deletion_info.deleted_rows()) {
      // In order to perform URL normalization, NormalizeId requires any omnibox
      // item type as argument. Pass kOmniboxGeneric here as we don't know the
      // specific type.
      query_based_mixed_types_ranker_->RemoveTarget(
          NormalizeId(row.url().spec(), RankingItemType::kOmniboxGeneric));
    }
  }

  // Force a save to disk. It is possible to get many calls to OnURLsDeleted in
  // quick succession, eg. when all history is cleared from a different device.
  // So delay the save slightly and only perform one save for all updates during
  // that delay.
  if (!query_mixed_ranker_save_queued_) {
    query_mixed_ranker_save_queued_ = true;
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SearchResultRanker::SaveQueryMixedRankerAfterDelete,
                       weak_factory_.GetWeakPtr()),
        TimeDelta::FromSeconds(3));
  }
}

void SearchResultRanker::SaveQueryMixedRankerAfterDelete() {
  query_based_mixed_types_ranker_->SaveToDisk();
  query_mixed_ranker_save_queued_ = false;
}

void SearchResultRanker::LogZeroStateResultScore(RankingItemType type,
                                                 float score) {
  const auto& now = Time::Now();
  if (type == RankingItemType::kOmniboxGeneric ||
      type == RankingItemType::kOmniboxSearch) {
    if (now - time_of_last_omnibox_log_ < kMinTimeBetweenLogs)
      return;
    time_of_last_omnibox_log_ = now;
    LogZeroStateReceivedScore("OmniboxSearch", score, 0.0f, 1.0f);
  } else if (type == RankingItemType::kZeroStateFile) {
    if (now - time_of_last_local_file_log_ < kMinTimeBetweenLogs)
      return;
    time_of_last_local_file_log_ = now;
    LogZeroStateReceivedScore("ZeroStateFile", score, 0.0f, 1.0f);
  } else if (type == RankingItemType::kDriveQuickAccess) {
    if (now - time_of_last_drive_log_ < kMinTimeBetweenLogs)
      return;
    time_of_last_drive_log_ = now;
    LogZeroStateReceivedScore("DriveQuickAccess", score, -10.0f, 10.0f);
  }
}

}  // namespace app_list
