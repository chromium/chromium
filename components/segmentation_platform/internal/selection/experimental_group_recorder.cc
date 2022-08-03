// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/experimental_group_recorder.h"

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metric_filter_utils.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"

#include "base/logging.h"

namespace segmentation_platform {

ExperimentalGroupRecorder::ExperimentalGroupRecorder(
    SegmentResultProvider* result_provider,
    FieldTrialRegister* field_trial_register,
    const std::string& segmentation_key,
    proto::SegmentId selected_segment)
    : field_trial_register_(field_trial_register),
      segmentation_key_(segmentation_key),
      segment_id_(selected_segment) {
  auto options = std::make_unique<SegmentResultProvider::GetResultOptions>();
  options->segmentation_key =
      base::StrCat({segmentation_key, kSubsegmentDiscreteMappingSuffix});
  options->segment_id = selected_segment;
  options->callback = base::BindOnce(&ExperimentalGroupRecorder::OnGetSegment,
                                     weak_ptr_factory_.GetWeakPtr());
  options->ignore_db_scores = false;
  result_provider->GetSegmentResult(std::move(options));
}

ExperimentalGroupRecorder::~ExperimentalGroupRecorder() = default;

void ExperimentalGroupRecorder::OnGetSegment(
    std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
  const std::string trial_name = stats::SegmentationKeyToSubsegmentTrialName(
      segmentation_key_, segment_id_);
  int rank = 0;
  if (result && result->rank) {
    rank = *result->rank;
  }

  // Can be nullptr in tests.
  if (field_trial_register_) {
    field_trial_register_->RegisterSubsegmentFieldTrialIfNeeded(
        trial_name, segment_id_, rank);
  }
}

}  // namespace segmentation_platform
