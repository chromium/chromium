// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/experimental_group_recorder.h"

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/metric_filter_utils.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"

#include "base/logging.h"

namespace segmentation_platform {

ExperimentalGroupRecorder::ExperimentalGroupRecorder(
    SegmentInfoDatabase* segment_database,
    FieldTrialRegister* field_trial_register,
    const std::string& segmentation_key,
    optimization_guide::proto::OptimizationTarget selected_segment)
    : field_trial_register_(field_trial_register),
      segmentation_key_(segmentation_key) {
  segment_database->GetSegmentInfo(
      selected_segment, base::BindOnce(&ExperimentalGroupRecorder::OnGetSegment,
                                       weak_ptr_factory_.GetWeakPtr()));
}

ExperimentalGroupRecorder::~ExperimentalGroupRecorder() = default;

void ExperimentalGroupRecorder::OnGetSegment(
    absl::optional<proto::SegmentInfo> result) {
  if (!result || !result->has_prediction_result()) {
    return;
  }
  const float score = result->prediction_result().result();
  std::string subsegment_key =
      base::StrCat({segmentation_key_, kSubsegmentDiscreteMappingSuffix});
  auto iter = result->model_metadata().discrete_mappings().find(subsegment_key);
  if (iter == result->model_metadata().discrete_mappings().end()) {
    // TODO(ssid): Move this check into ConvertToDiscreteScore().
    return;
  }
  const int rank = metadata_utils::ConvertToDiscreteScore(
      base::StrCat({segmentation_key_, kSubsegmentDiscreteMappingSuffix}),
      score, result->model_metadata());

  const std::string trial_name = stats::SegmentationKeyToSubsegmentTrialName(
      segmentation_key_, result->segment_id());

  // Can be nullptr in tests.
  if (field_trial_register_) {
    field_trial_register_->RegisterSubsegmentFieldTrialIfNeeded(
        trial_name, result->segment_id(), rank);
  }
}

}  // namespace segmentation_platform
