// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_EXPERIMENTAL_GROUP_RECORDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_EXPERIMENTAL_GROUP_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

struct Config;
class FieldTrialRegister;

// Records experimental sub groups for the given optimization target.
class ExperimentalGroupRecorder {
 public:
  // On construction, gets the model score from the database and records the
  // subsegment based on the score. This class must be kept alive till the
  // recording is complete, can be used only once.
  ExperimentalGroupRecorder(SegmentResultProvider* result_provider,
                            FieldTrialRegister* field_trial_register,
                            const Config& config,
                            proto::SegmentId segment_id);
  ~ExperimentalGroupRecorder();

  ExperimentalGroupRecorder(const ExperimentalGroupRecorder&) = delete;
  ExperimentalGroupRecorder& operator=(const ExperimentalGroupRecorder&) =
      delete;

 private:
  void OnGetSegment(
      std::unique_ptr<SegmentResultProvider::SegmentResult> result);

  const raw_ptr<FieldTrialRegister> field_trial_register_;
  const std::string subsegment_trial_name_;
  const proto::SegmentId segment_id_;

  base::WeakPtrFactory<ExperimentalGroupRecorder> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_EXPERIMENTAL_GROUP_RECORDER_H_
