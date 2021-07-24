// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

SegmentSelectorImpl::SegmentSelectorImpl(SegmentInfoDatabase* segment_database,
                                         SegmentationResultPrefs* result_prefs,
                                         Config* config)
    : segment_database_(segment_database),
      result_prefs_(result_prefs),
      config_(config),
      initialized_(false) {
  // Read selected segment from prefs.
  const auto& selected_segment =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);
  if (selected_segment.has_value()) {
    selected_segment_last_session_.segment = selected_segment->segment_id;
    selected_segment_last_session_.is_ready = true;
  }
}

SegmentSelectorImpl::~SegmentSelectorImpl() = default;

void SegmentSelectorImpl::Initialize(base::OnceClosure callback) {
  // Read model results from DB.
  segment_database_->GetSegmentInfoForSegments(
      config_->segment_ids,
      base::BindOnce(&SegmentSelectorImpl::ReadScoresFromLastSession,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentSelectorImpl::GetSelectedSegment(
    SegmentSelectionCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), selected_segment_last_session_));
}

void SegmentSelectorImpl::GetSegmentScore(
    OptimizationTarget segment_id,
    SingleSegmentResultCallback callback) {
  DCHECK(initialized_);

  absl::optional<float> score;
  auto iter = segment_score_last_session_.find(segment_id);
  if (iter != segment_score_last_session_.end())
    score = iter->second;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), score));
}

void SegmentSelectorImpl::OnSegmentUsed(OptimizationTarget segment_id) {
  DCHECK(initialized_);
  // TODO(shaktisahu): Implement this.
}

void SegmentSelectorImpl::OnModelExecutionCompleted(
    OptimizationTarget segment_id) {
  segment_database_->GetSegmentInfoForSegments(
      config_->segment_ids,
      base::BindOnce(&SegmentSelectorImpl::FindBestSegment,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SegmentSelectorImpl::FindBestSegment(
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        all_segments) {
  int max_score = 0;
  OptimizationTarget max_score_id =
      OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN;
  // Loop through all the results. Convert them to discrete scores. Select the
  // one with highest discrete score.
  for (const auto& pair : all_segments) {
    OptimizationTarget id = pair.first;
    const proto::SegmentInfo& info = pair.second;
    if (id == OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN)
      continue;

    if (!info.has_prediction_result())
      continue;

    int score = ConvertToDiscreteScore(id, config_->segmentation_key,
                                       info.prediction_result().result(),
                                       info.model_metadata());
    if (score > max_score) {
      max_score = score;
      max_score_id = id;
    } else if (score == max_score && score > 0) {
      // TODO(shaktisahu): Use fallback priority.
    }
  }

  UpdateSelectedSegment(max_score_id);
}

void SegmentSelectorImpl::UpdateSelectedSegment(
    OptimizationTarget new_selection) {
  const auto& previous_selection =
      result_prefs_->ReadSegmentationResultFromPref(config_->segmentation_key);

  bool skip_updating_prefs = false;
  if (previous_selection.has_value()) {
    skip_updating_prefs =
        new_selection == previous_selection->segment_id ||
        (previous_selection->selection_time + config_->segment_selection_ttl >
         base::Time::Now());
    // TODO(shaktisahu): Use segment selection inertia.
  }

  stats::RecordSegmentSelectionComputed(
      new_selection, previous_selection.has_value()
                         ? absl::make_optional(previous_selection->segment_id)
                         : absl::nullopt);

  if (skip_updating_prefs)
    return;

  // Write result to prefs. Delete if no valid selection.
  absl::optional<SelectedSegment> updated_selection;
  if (new_selection != OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
    updated_selection = absl::make_optional<SelectedSegment>(new_selection);
    updated_selection->selection_time = base::Time::Now();
  }
  result_prefs_->SaveSegmentationResultToPref(config_->segmentation_key,
                                              updated_selection);
}

void SegmentSelectorImpl::ReadScoresFromLastSession(
    base::OnceClosure callback,
    std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>
        all_segments) {
  // Read results from last session to memory.
  for (const auto& pair : all_segments) {
    OptimizationTarget id = pair.first;
    const proto::SegmentInfo& info = pair.second;
    if (!info.has_prediction_result())
      continue;

    float result = info.prediction_result().result();
    segment_score_last_session_.emplace(std::make_pair(id, result));
  }

  initialized_ = true;
  std::move(callback).Run();
}

int SegmentSelectorImpl::ConvertToDiscreteScore(
    OptimizationTarget segment_id,
    const std::string& mapping_key,
    float score,
    const proto::SegmentationModelMetadata& metadata) {
  auto iter = metadata.discrete_mappings().find(mapping_key);
  if (iter == metadata.discrete_mappings().end()) {
    iter =
        metadata.discrete_mappings().find(metadata.default_discrete_mapping());
    if (iter == metadata.discrete_mappings().end())
      return 0;
  }
  DCHECK(iter != metadata.discrete_mappings().end());

  const auto& mapping = iter->second;

  // Iterate over the entries and find the last entry whose min result is equal
  // to or less than the input.
  int discrete_result = 0;
  for (int i = 0; i < mapping.entries_size(); i++) {
    const auto& entry = mapping.entries(i);
    if (score < entry.min_result())
      break;

    discrete_result = entry.rank();
  }

  return discrete_result;
}

}  // namespace segmentation_platform
