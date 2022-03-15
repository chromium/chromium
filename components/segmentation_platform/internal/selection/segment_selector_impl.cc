// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

using optimization_guide::proto::OptimizationTarget_Name;

SegmentSelectorImpl::SegmentSelectorImpl(
    SegmentInfoDatabase* segment_database,
    SignalStorageConfig* signal_storage_config,
    SegmentationResultPrefs* result_prefs,
    const Config* config,
    base::Clock* clock,
    const PlatformOptions& platform_options)
    : segment_database_(segment_database),
      signal_storage_config_(signal_storage_config),
      result_prefs_(result_prefs),
      config_(config),
      clock_(clock),
      platform_options_(platform_options) {
  // Read selected segment from prefs.
  const auto& selected_segment =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);
  if (selected_segment.has_value()) {
    selected_segment_last_session_.segment = selected_segment->segment_id;
    selected_segment_last_session_.is_ready = true;
    stats::RecordSegmentSelectionFailure(
        config_->segmentation_key,
        stats::SegmentationSelectionFailureReason::kSelectionAvailableInPrefs);
  } else {
    stats::RecordSegmentSelectionFailure(
        config_->segmentation_key, stats::SegmentationSelectionFailureReason::
                                       kInvalidSelectionResultInPrefs);
  }
}

SegmentSelectorImpl::~SegmentSelectorImpl() = default;

void SegmentSelectorImpl::GetSelectedSegment(
    SegmentSelectionCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), selected_segment_last_session_));
}

SegmentSelectionResult SegmentSelectorImpl::GetCachedSegmentResult() {
  return selected_segment_last_session_;
}

void SegmentSelectorImpl::OnModelExecutionCompleted(
    OptimizationTarget segment_id) {
  // If the |segment_id| is not in config, then skip any updates early.
  if (!base::Contains(config_->segment_ids, segment_id))
    return;

  segment_database_->GetSegmentInfoForSegments(
      config_->segment_ids,
      base::BindOnce(&SegmentSelectorImpl::RunSegmentSelection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentSelectorImpl::RunSegmentSelection(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> all_segments) {
  if (!CanComputeSegmentSelection(*all_segments))
    return;

  OptimizationTarget selected_segment = FindBestSegment(*all_segments);
  UpdateSelectedSegment(selected_segment);
}

bool SegmentSelectorImpl::CanComputeSegmentSelection(
    const SegmentInfoDatabase::SegmentInfoList& all_segments) {
  VLOG(1) << __func__ << ": all_segments.size()=" << all_segments.size();
  // Don't compute results if we don't have enough signals, or don't have
  // valid unexpired results for any of the segments.
  for (const auto& pair : all_segments) {
    const proto::SegmentInfo& segment_info = pair.second;
    if (!platform_options_.force_refresh_results &&
        !signal_storage_config_->MeetsSignalCollectionRequirement(
            segment_info.model_metadata())) {
      VLOG(1) << __func__ << ": segment="
              << OptimizationTarget_Name(segment_info.segment_id())
              << " does not meet signal collection requirements.";
      stats::RecordSegmentSelectionFailure(
          config_->segmentation_key, stats::SegmentationSelectionFailureReason::
                                         kAtLeastOneSegmentSignalsNotCollected);
      return false;
    }

    if (metadata_utils::HasExpiredOrUnavailableResult(segment_info,
                                                      clock_->Now())) {
      VLOG(1) << __func__ << ": segment="
              << OptimizationTarget_Name(segment_info.segment_id())
              << " has expired or unavailable result.";
      stats::RecordSegmentSelectionFailure(
          config_->segmentation_key, stats::SegmentationSelectionFailureReason::
                                         kAtLeastOneSegmentNotReady);
      return false;
    }
  }

  // Don't compute results if segment selection TTL hasn't expired.
  const auto& previous_selection =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);
  if (previous_selection.has_value()) {
    bool was_unknown_selected = previous_selection->segment_id ==
                                OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;
    base::TimeDelta ttl_to_use = was_unknown_selected
                                     ? config_->unknown_selection_ttl
                                     : config_->segment_selection_ttl;
    if (!platform_options_.force_refresh_results &&
        previous_selection->selection_time + ttl_to_use > clock_->Now()) {
      stats::RecordSegmentSelectionFailure(
          config_->segmentation_key,
          stats::SegmentationSelectionFailureReason::kSelectionTtlNotExpired);
      VLOG(1) << __func__ << ": previous selection of segment="
              << OptimizationTarget_Name(previous_selection->segment_id)
              << " has not yet expired.";
      return false;
    }
  }

  return true;
}

OptimizationTarget SegmentSelectorImpl::FindBestSegment(
    const SegmentInfoDatabase::SegmentInfoList& all_segments) {
  int max_score = 0;
  OptimizationTarget max_score_id =
      OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;
  // Loop through all the results. Convert them to discrete scores. Select the
  // one with highest discrete score.
  for (const auto& pair : all_segments) {
    OptimizationTarget id = pair.first;
    const proto::SegmentInfo& info = pair.second;
    int score = metadata_utils::ConvertToDiscreteScore(
        config_->segmentation_key, info.prediction_result().result(),
        info.model_metadata());
    VLOG(1) << __func__ << ": segment=" << OptimizationTarget_Name(id)
            << ": result=" << info.prediction_result().result()
            << ", score=" << score;
    if (score > max_score) {
      max_score = score;
      max_score_id = id;
    } else if (score == max_score && score > 0) {
      // TODO(shaktisahu): Use fallback priority.
    }
  }

  return max_score_id;
}

void SegmentSelectorImpl::UpdateSelectedSegment(
    OptimizationTarget new_selection) {
  VLOG(1) << __func__ << ": Updating selected segment="
          << OptimizationTarget_Name(new_selection);
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
        new_selection == OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;
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
