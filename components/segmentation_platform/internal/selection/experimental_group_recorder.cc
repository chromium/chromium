// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/experimental_group_recorder.h"

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"

namespace segmentation_platform {

ExperimentalGroupRecorder::ExperimentalGroupRecorder(
    SegmentResultProvider* result_provider,
    FieldTrialRegister* field_trial_register,
    const Config& config,
    proto::SegmentId segment_id)
    : field_trial_register_(field_trial_register),
      subsegment_trial_name_(
          base::StrCat({config.GetSegmentationFilterName(), "_",
                        config.GetSegmentUmaName(segment_id)})),
      segment_id_(segment_id) {
  auto options = std::make_unique<SegmentResultProvider::GetResultOptions>();
  options->discrete_mapping_key =
      base::StrCat({config.segmentation_key, kSubsegmentDiscreteMappingSuffix});
  options->segment_id = segment_id;
  options->callback = base::BindOnce(&ExperimentalGroupRecorder::OnGetSegment,
                                     weak_ptr_factory_.GetWeakPtr());
  options->ignore_db_scores = false;
  options->save_results_to_db = true;
  result_provider->GetSegmentResult(std::move(options));
}

ExperimentalGroupRecorder::~ExperimentalGroupRecorder() = default;

void ExperimentalGroupRecorder::OnGetSegment(
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  int rank = 0;
  if (result && result->rank) {
    rank = *result->rank;
  }

  // Can be nullptr in tests.
  if (field_trial_register_) {
    field_trial_register_->RegisterSubsegmentFieldTrialIfNeeded(
        subsegment_trial_name_, segment_id_, rank);
  }
}

}  // namespace segmentation_platform
