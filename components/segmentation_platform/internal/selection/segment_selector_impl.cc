// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/selection/experimental_group_recorder.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform {
namespace {

stats::SegmentationSelectionFailureReason GetFailureReason(
    SegmentResultProvider::ResultState result_state) {
  switch (result_state) {
    case SegmentResultProvider::ResultState::kUnknown:
    case SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed:
    case SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed:
    case SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed:
    case SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed:
      NOTREACHED_IN_MIGRATION();
      return stats::SegmentationSelectionFailureReason::kMaxValue;
    case SegmentResultProvider::ResultState::kServerModelDatabaseScoreNotReady:
      return stats::SegmentationSelectionFailureReason::
          kServerModelDatabaseScoreNotReady;
    case SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreNotReady:
      return stats::SegmentationSelectionFailureReason::
          kDefaultModelDatabaseScoreNotReady;
    case SegmentResultProvider::ResultState::
        kServerModelSegmentInfoNotAvailable:
      return stats::SegmentationSelectionFailureReason::
          kServerModelSegmentInfoNotAvailable;
    case SegmentResultProvider::ResultState::
        kDefaultModelSegmentInfoNotAvailable:
      return stats::SegmentationSelectionFailureReason::
          kDefaultModelSegmentInfoNotAvailable;
    case SegmentResultProvider::ResultState::kServerModelSignalsNotCollected:
      return stats::SegmentationSelectionFailureReason::
          kServerModelSignalsNotCollected;
    case SegmentResultProvider::ResultState::kDefaultModelSignalsNotCollected:
      return stats::SegmentationSelectionFailureReason::
          kDefaultModelSignalsNotCollected;
    case SegmentResultProvider::ResultState::kDefaultModelExecutionFailed:
      return stats::SegmentationSelectionFailureReason::
          kDefaultModelExecutionFailed;
    case SegmentResultProvider::ResultState::kServerModelExecutionFailed:
      return stats::SegmentationSelectionFailureReason::
          kServerModelExecutionFailed;
  }
}

SegmentSelectionResult MakeResultFromSelection(
    const SelectedSegment& selection) {
  SegmentSelectionResult result;
  result.segment = selection.segment_id;
  result.is_ready = true;
  result.rank = selection.rank;
  return result;
}

}  // namespace

using proto::SegmentId_Name;

SegmentSelectorImpl::SegmentSelectorImpl(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    PrefService* pref_service,
    const Config* config,
    FieldTrialRegister* field_trial_register,
    base::Clock* clock,
    const PlatformOptions& platform_options)
    : SegmentSelectorImpl(
          segment_database,
          signal_storage_config,
          std::make_unique<SegmentationResultPrefs>(pref_service),
          config,
          field_trial_register,
          clock,
          platform_options) {}

SegmentSelectorImpl::SegmentSelectorImpl(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    std::unique_ptr<SegmentationResultPrefs> prefs,
    const Config* config,
    FieldTrialRegister* field_trial_register,
    base::Clock* clock,
    const PlatformOptions& platform_options)
    : result_prefs_(std::move(prefs)),
      segment_database_(segment_database),
      signal_storage_config_(signal_storage_config),
      config_(config),
      field_trial_register_(field_trial_register),
      clock_(clock),
      platform_options_(platform_options) {
  // Read selected segment from prefs.
  const auto& selected_segment =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);
  if (selected_segment.has_value()) {
    selected_segment_ = MakeResultFromSelection(*selected_segment);
    stats::RecordSegmentSelectionFailure(
        *config_,
        stats::SegmentationSelectionFailureReason::kSelectionAvailableInPrefs);

  } else {
    stats::RecordSegmentSelectionFailure(
        *config_, stats::SegmentationSelectionFailureReason::
                      kInvalidSelectionResultInPrefs);
  }
  RecordFieldTrials();
}

SegmentSelectorImpl::~SegmentSelectorImpl() = default;

void SegmentSelectorImpl::OnPlatformInitialized(
    ExecutionService* execution_service) {
  // If training data collector has been set for testing, do not get it from
  // execution service.
  if (!training_data_collector_) {
    training_data_collector_ = execution_service->training_data_collector();
  }
  segment_result_provider_ = SegmentResultProvider::Create(
      segment_database_, signal_storage_config_, execution_service, clock_,
      platform_options_.force_refresh_results);
  if (IsPreviousSelectionInvalid()) {
    SelectSegmentAndStoreToPrefs();
  }

  // If the segment selection is ready, also record the subsegment for all the
  // segments.
  // TODO(ssid): Store the scores in prefs so that this can be recorded earlier
  // in startup.
  if (selected_segment_.is_ready) {
    for (const auto& segment_id : config_->segments) {
      experimental_group_recorder_.emplace_back(
          std::make_unique<ExperimentalGroupRecorder>(
              segment_result_provider_.get(), field_trial_register_, *config_,
              segment_id.first));
    }
  }
}

void SegmentSelectorImpl::GetSelectedSegment(
    SegmentSelectionCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), selected_segment_));
  used_result_in_current_session_ = true;
}

SegmentSelectionResult SegmentSelectorImpl::GetCachedSegmentResult() {
  used_result_in_current_session_ = true;
  return selected_segment_;
}

void SegmentSelectorImpl::OnModelExecutionCompleted(SegmentId segment_id) {
  DCHECK(segment_result_provider_);

  // If the |segment_id| is not in config, then skip any updates early.
  if (!base::Contains(config_->segments, segment_id))
    return;

  if (!IsPreviousSelectionInvalid())
    return;

  SelectSegmentAndStoreToPrefs();
}

bool SegmentSelectorImpl::IsPreviousSelectionInvalid() {
  // Don't compute results if segment selection TTL hasn't expired.
  const auto& previous_selection =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);
  if (previous_selection.has_value()) {
    bool was_unknown_selected = previous_selection->segment_id ==
                                SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
    base::TimeDelta ttl_to_use = was_unknown_selected
                                     ? config_->unknown_selection_ttl
                                     : config_->segment_selection_ttl;
    if (!platform_options_.force_refresh_results &&
        previous_selection->selection_time + ttl_to_use > clock_->Now()) {
      stats::RecordSegmentSelectionFailure(
          *config_,
          stats::SegmentationSelectionFailureReason::kSelectionTtlNotExpired);
      VLOG(1) << __func__ << ": previous selection of segment="
              << SegmentId_Name(previous_selection->segment_id)
              << " has not yet expired.";
      return false;
    }
  }

  return true;
}

void SegmentSelectorImpl::SelectSegmentAndStoreToPrefs() {
  if (!config_->auto_execute_and_cache) {
    return;
  }
  GetRankForNextSegment(std::make_unique<SegmentRanks>(), nullptr,
                        SegmentSelectionCallback());
}

void SegmentSelectorImpl::GetRankForNextSegment(
    std::unique_ptr<SegmentRanks> ranks,
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback) {
  for (const auto& needed_segment : config_->segments) {
    if (ranks->count(needed_segment.first) == 0) {
      auto options =
          std::make_unique<SegmentResultProvider::GetResultOptions>();
      options->segment_id = needed_segment.first;
      options->discrete_mapping_key = config_->segmentation_key;
      options->ignore_db_scores = !config_->auto_execute_and_cache;
      options->save_results_to_db = true;
      options->input_context = input_context;
      options->callback = base::BindOnce(
          &SegmentSelectorImpl::OnGetResultForSegmentSelection,
          weak_ptr_factory_.GetWeakPtr(), std::move(ranks), input_context,
          std::move(callback), needed_segment.first);

      segment_result_provider_->GetSegmentResult(std::move(options));
      return;
    }
  }

  // Finished fetching ranks for all segments.
  auto segment_id_and_rank = FindBestSegment(*ranks);
  if (!config_->auto_execute_and_cache) {
    DCHECK(!callback.is_null());
    SegmentSelectionResult result;
    result.is_ready = true;
    result.segment = segment_id_and_rank.first;
    result.rank = segment_id_and_rank.second;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
    stats::RecordSegmentSelectionComputed(*config_, segment_id_and_rank.first,
                                          std::nullopt);
  } else {
    DCHECK(callback.is_null());
    UpdateSelectedSegment(segment_id_and_rank.first,
                          segment_id_and_rank.second);
  }
}

void SegmentSelectorImpl::OnGetResultForSegmentSelection(
    std::unique_ptr<SegmentRanks> ranks,
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback,
    SegmentId current_segment_id,
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  if (!result->rank) {
    stats::RecordSegmentSelectionFailure(*config_,
                                         GetFailureReason(result->state));
    if (!config_->auto_execute_and_cache && !callback.is_null()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), SegmentSelectionResult()));
    }
    return;
  }
  ranks->insert(std::make_pair(current_segment_id, *result->rank));

  if (!config_->auto_execute_and_cache && training_data_collector_) {
    // Collect training data on demand.
    training_data_collector_->OnDecisionTime(
        current_segment_id, input_context,
        proto::TrainingOutputs::TriggerConfig::ONDEMAND,
        std::move(result->model_inputs));
  }

  GetRankForNextSegment(std::move(ranks), input_context, std::move(callback));
}

std::pair<SegmentId, float> SegmentSelectorImpl::FindBestSegment(
    const SegmentRanks& segment_results) {
  const float kMinRank = 0;
  float max_rank = kMinRank;
  SegmentId max_rank_id = SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
  // Loop through all the results. Convert them to discrete ranks. Select the
  // one with highest discrete rank.
  for (const auto& pair : segment_results) {
    SegmentId id = pair.first;
    float rank = pair.second;
    if (rank > max_rank) {
      max_rank = rank;
      max_rank_id = id;
    } else if (rank == max_rank && rank > kMinRank) {
      // TODO(shaktisahu): Use fallback priority.
    }
  }

  return std::make_pair(max_rank_id, max_rank);
}

void SegmentSelectorImpl::UpdateSelectedSegment(SegmentId new_selection,
                                                float rank) {
  VLOG(1) << __func__
          << ": Updating selected segment=" << SegmentId_Name(new_selection)
          << " rank=" << rank;
  const auto& previous_selection =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);

  // Auto-extend the results, if
  // (1) segment selection and rank hasn't changed.
  // (2) or, UNKNOWN selection TTL = 0 and the new segment is UNKNOWN, and the
  //     previous one was a valid one.
  bool skip_updating_prefs = false;
  if (previous_selection.has_value()) {
    skip_updating_prefs =
        new_selection == previous_selection->segment_id &&
        (previous_selection->rank && rank == *previous_selection->rank);
    skip_updating_prefs |=
        config_->unknown_selection_ttl == base::TimeDelta() &&
        new_selection == SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
    // TODO(shaktisahu): Use segment selection inertia.
  }

  stats::RecordSegmentSelectionComputed(
      *config_, new_selection,
      previous_selection.has_value()
          ? std::make_optional(previous_selection->segment_id)
          : std::nullopt);

  VLOG(1) << __func__ << " Key=" << config_->segmentation_key
          << " : skip_updating_prefs=" << skip_updating_prefs;
  if (skip_updating_prefs)
    return;

  // Write result to prefs.
  auto updated_selection =
      std::make_optional<SelectedSegment>(new_selection, rank);
  updated_selection->selection_time = clock_->Now();

  result_prefs_->SaveSegmentationResultToPref(config_->segmentation_key,
                                              updated_selection);

  if (!used_result_in_current_session_) {
    selected_segment_ = MakeResultFromSelection(*updated_selection);
    RecordFieldTrials();
  }

  for (const auto& segment : config_->segments) {
    training_data_collector_->OnDecisionTime(
        segment.first, nullptr, proto::TrainingOutputs::TriggerConfig::PERIODIC,
        std::nullopt, /*decision_result_update_trigger=*/true);
  }
}

void SegmentSelectorImpl::RecordFieldTrials() const {
  // Register can be nullptr in tests.
  if (!config_->auto_execute_and_cache || !field_trial_register_) {
    return;
  }
  const std::string& trial_name = config_->GetSegmentationFilterName();
  std::string group_name;
  if (selected_segment_.is_ready) {
    group_name = config_->GetSegmentUmaName(*selected_segment_.segment);
  } else {
    group_name = "Unselected";
  }
  field_trial_register_->RegisterFieldTrial(trial_name, group_name);
}

void SegmentSelectorImpl::CallbackWrapper(
    base::Time start_time,
    SegmentSelectionCallback callback,
    const SegmentSelectionResult& result) {
  stats::RecordOnDemandSegmentSelectionDuration(*config_, result,
                                                base::Time::Now() - start_time);
  std::move(callback).Run(result);
}

}  // namespace segmentation_platform
