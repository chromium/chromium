// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metric_filter_utils.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/selection/experimental_group_recorder.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {
namespace {

stats::SegmentationSelectionFailureReason GetFailureReason(
    SegmentResultProvider::ResultState result_state) {
  switch (result_state) {
    case SegmentResultProvider::ResultState::kUnknown:
    case SegmentResultProvider::ResultState::kSuccessFromDatabase:
    case SegmentResultProvider::ResultState::kDefaultModelScoreUsed:
    case SegmentResultProvider::ResultState::kTfliteModelScoreUsed:
      NOTREACHED();
      return stats::SegmentationSelectionFailureReason::kMaxValue;
    case SegmentResultProvider::ResultState::kDatabaseScoreNotReady:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentNotReady;
    case SegmentResultProvider::ResultState::kSegmentNotAvailable:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentNotAvailable;
    case SegmentResultProvider::ResultState::kSignalsNotCollected:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentSignalsNotCollected;
    case SegmentResultProvider::ResultState::kDefaultModelMetadataMissing:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentDefaultMissingMetadata;
    case SegmentResultProvider::ResultState::kDefaultModelSignalNotCollected:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentDefaultSignalNotCollected;
    case SegmentResultProvider::ResultState::kDefaultModelExecutionFailed:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentDefaultExecFailed;
    case SegmentResultProvider::ResultState::kTfliteModelExecutionFailed:
      return stats::SegmentationSelectionFailureReason::
          kAtLeastOneSegmentTfliteExecFailed;
  }
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
    const PlatformOptions& platform_options,
    DefaultModelManager* default_model_manager)
    : SegmentSelectorImpl(
          segment_database,
          signal_storage_config,
          std::make_unique<SegmentationResultPrefs>(pref_service),
          config,
          field_trial_register,
          clock,
          platform_options,
          default_model_manager) {}

SegmentSelectorImpl::SegmentSelectorImpl(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    std::unique_ptr<SegmentationResultPrefs> prefs,
    const Config* config,
    FieldTrialRegister* field_trial_register,
    base::Clock* clock,
    const PlatformOptions& platform_options,
    DefaultModelManager* default_model_manager)
    : result_prefs_(std::move(prefs)),
      segment_database_(segment_database),
      signal_storage_config_(signal_storage_config),
      default_model_manager_(default_model_manager),
      config_(config),
      field_trial_register_(field_trial_register),
      clock_(clock),
      platform_options_(platform_options) {
  // Read selected segment from prefs.
  const auto& selected_segment =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);
  std::string trial_name =
      stats::SegmentationKeyToTrialName(config_->segmentation_key);
  std::string group_name;
  if (selected_segment.has_value()) {
    selected_segment_last_session_.segment = selected_segment->segment_id;
    selected_segment_last_session_.is_ready = true;
    stats::RecordSegmentSelectionFailure(
        config_->segmentation_key,
        stats::SegmentationSelectionFailureReason::kSelectionAvailableInPrefs);

    group_name = stats::OptimizationTargetToSegmentGroupName(
        selected_segment->segment_id);
  } else {
    stats::RecordSegmentSelectionFailure(
        config_->segmentation_key, stats::SegmentationSelectionFailureReason::
                                       kInvalidSelectionResultInPrefs);
    group_name = "Unselected";
  }

  // Can be nullptr in tests.
  if (field_trial_register_) {
    field_trial_register_->RegisterFieldTrial(trial_name, group_name);
  }
}

SegmentSelectorImpl::~SegmentSelectorImpl() = default;

void SegmentSelectorImpl::OnPlatformInitialized(
    ExecutionService* execution_service) {
  segment_result_provider_ = SegmentResultProvider::Create(
      segment_database_, signal_storage_config_, default_model_manager_,
      execution_service, clock_, platform_options_.force_refresh_results);
  if (IsPreviousSelectionInvalid()) {
    SelectSegmentAndStoreToPrefs();
  }

  // If the segment selection is ready, also record the subsegment for all the
  // segments.
  // TODO(ssid): Store the scores in prefs so that this can be recorded earlier
  // in startup.
  if (selected_segment_last_session_.is_ready) {
    for (const SegmentId segment_id : config_->segment_ids) {
      experimental_group_recorder_.emplace_back(
          std::make_unique<ExperimentalGroupRecorder>(
              segment_result_provider_.get(), field_trial_register_,
              config_->segmentation_key, segment_id));
    }
  }
}

void SegmentSelectorImpl::GetSelectedSegment(
    SegmentSelectionCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), selected_segment_last_session_));
}

SegmentSelectionResult SegmentSelectorImpl::GetCachedSegmentResult() {
  return selected_segment_last_session_;
}

void SegmentSelectorImpl::GetSelectedSegmentOnDemand(
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback) {
  DCHECK(config_->on_demand_execution);
  GetRankForNextSegment(std::make_unique<SegmentRanks>(), input_context,
                        std::move(callback));
}

void SegmentSelectorImpl::OnModelExecutionCompleted(SegmentId segment_id) {
  DCHECK(segment_result_provider_);

  // If the |segment_id| is not in config, then skip any updates early.
  if (!base::Contains(config_->segment_ids, segment_id))
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
          config_->segmentation_key,
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
  if (config_->on_demand_execution) {
    return;
  }
  GetRankForNextSegment(std::make_unique<SegmentRanks>(), nullptr,
                        SegmentSelectionCallback());
}

void SegmentSelectorImpl::GetRankForNextSegment(
    std::unique_ptr<SegmentRanks> ranks,
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback) {
  for (SegmentId needed_segment : config_->segment_ids) {
    if (ranks->count(needed_segment) == 0) {
      auto options =
          std::make_unique<SegmentResultProvider::GetResultOptions>();
      options->segment_id = needed_segment;
      options->segmentation_key = config_->segmentation_key;
      options->ignore_db_scores = config_->on_demand_execution;
      options->callback =
          base::BindOnce(&SegmentSelectorImpl::OnGetResultForSegmentSelection,
                         weak_ptr_factory_.GetWeakPtr(), std::move(ranks),
                         input_context, std::move(callback), needed_segment);

      segment_result_provider_->GetSegmentResult(std::move(options));
      return;
    }
  }

  // Finished fetching ranks for all segments.
  SegmentId selected_segment = FindBestSegment(*ranks);
  if (config_->on_demand_execution) {
    DCHECK(!callback.is_null());
    SegmentSelectionResult result;
    result.is_ready = true;
    result.segment = selected_segment;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  } else {
    DCHECK(callback.is_null());
    UpdateSelectedSegment(selected_segment);
  }
}

void SegmentSelectorImpl::OnGetResultForSegmentSelection(
    std::unique_ptr<SegmentRanks> ranks,
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback,
    SegmentId current_segment_id,
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  if (!result->rank) {
    stats::RecordSegmentSelectionFailure(config_->segmentation_key,
                                         GetFailureReason(result->state));
    return;
  }
  ranks->insert(std::make_pair(current_segment_id, *result->rank));

  GetRankForNextSegment(std::move(ranks), input_context, std::move(callback));
}

SegmentId SegmentSelectorImpl::FindBestSegment(
    const SegmentRanks& segment_results) {
  int max_rank = 0;
  SegmentId max_rank_id = SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
  // Loop through all the results. Convert them to discrete ranks. Select the
  // one with highest discrete rank.
  for (const auto& pair : segment_results) {
    SegmentId id = pair.first;
    int rank = pair.second;
    if (rank > max_rank) {
      max_rank = rank;
      max_rank_id = id;
    } else if (rank == max_rank && rank > 0) {
      // TODO(shaktisahu): Use fallback priority.
    }
  }

  return max_rank_id;
}

void SegmentSelectorImpl::UpdateSelectedSegment(SegmentId new_selection) {
  VLOG(1) << __func__
          << ": Updating selected segment=" << SegmentId_Name(new_selection);
  const auto& previous_selection =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);

  // Auto-extend the results, if
  // (1) segment selection hasn't changed.
  // (2) or, UNKNOWN selection TTL = 0 and the new segment is UNKNOWN, and the
  //     previous one was a valid one.
  bool skip_updating_prefs = false;
  if (previous_selection.has_value()) {
    skip_updating_prefs = new_selection == previous_selection->segment_id;
    skip_updating_prefs |=
        config_->unknown_selection_ttl == base::TimeDelta() &&
        new_selection == SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
    // TODO(shaktisahu): Use segment selection inertia.
  }

  stats::RecordSegmentSelectionComputed(
      config_->segmentation_key, new_selection,
      previous_selection.has_value()
          ? absl::make_optional(previous_selection->segment_id)
          : absl::nullopt);

  VLOG(1) << __func__ << ": skip_updating_prefs=" << skip_updating_prefs;
  if (skip_updating_prefs)
    return;

  // Write result to prefs.
  auto updated_selection = absl::make_optional<SelectedSegment>(new_selection);
  updated_selection->selection_time = clock_->Now();

  result_prefs_->SaveSegmentationResultToPref(config_->segmentation_key,
                                              updated_selection);
}

}  // namespace segmentation_platform
