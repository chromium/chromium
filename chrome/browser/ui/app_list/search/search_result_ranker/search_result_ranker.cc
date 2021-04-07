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
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
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

SearchResultRanker::SearchResultRanker(Profile* profile)
    : profile_(profile), weak_factory_(this) {
  DCHECK(profile);
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

  app_launch_event_logger_ = std::make_unique<app_list::AppLaunchEventLogger>();

  // Initialize on-device app ranking model.
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

void SearchResultRanker::FetchRankings(const std::u16string& query) {
  last_query_ = query;

  // The search controller potentially calls SearchController::FetchResults
  // several times for each user's search, so we cache the results of querying
  // the models for a short time, to prevent unecessary queries.
  const auto& now = Time::Now();
  if (now - time_of_last_fetch_ < kMinSecondsBetweenFetches)
    return;
  time_of_last_fetch_ = now;

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

  // Counts how many times a given type has been seen so far in the results
  // list.
  base::flat_map<RankingItemType, int> zero_state_type_counts;

  for (auto& result : *results) {
    const auto& type = RankingItemTypeFromSearchResult(*result.result);
    const auto& model = ModelForType(type);

    if (model == Model::MIXED_TYPES) {
      if (last_query_.empty() && zero_state_group_ranker_) {
        ScoreZeroStateItem(&result, type, &zero_state_type_counts);
      }
    } else if (model == Model::APPS) {
      // Do not rerank apps for a query-based search.
      if (!last_query_.empty())
        continue;

      if (app_ranker_) {
        const auto& it = app_ranks_.find(NormalizeAppId(result.result->id()));
        if (it != app_ranks_.end()) {
          result.score = kBoostOfApps + ReRange(it->second, 0.67, 1.0);
        }
      }
    }
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

  auto model = ModelForType(app_launch_data.ranking_item_type);
  if (model == Model::MIXED_TYPES) {
    // We currently only have a mixed types model for zero-state, so stop if
    // the launch has a query attached.
    if (!app_launch_data.query.empty())
      return;

    LogZeroStateLaunchType(app_launch_data.ranking_item_type);
    if (zero_state_group_ranker_) {
      zero_state_group_ranker_->Record(base::NumberToString(
          static_cast<int>(app_launch_data.ranking_item_type)));
    }
  } else if (model == Model::APPS && app_ranker_) {
    app_ranker_->Record(NormalizeAppId(app_launch_data.id));
  }
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

    // Override the result at |next_modifiable_index| with
    // |candidate_override_index| by swapping their scores.
    std::swap(result_ptrs[candidate_override_index]->score,
              result_ptrs[next_modifiable_index]->score);
    --next_modifiable_index;
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

}  // namespace app_list
