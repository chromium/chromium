// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_EXPERIMENTAL_GROUP_RECORDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_EXPERIMENTAL_GROUP_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

class SegmentInfoDatabase;
class FieldTrialRegister;

// Records experimental sub groups for the given optimization target.
class ExperimentalGroupRecorder {
 public:
  // On construction, gets the model score from the database and records the
  // subsegment based on the score. This class must be kept alive till the
  // recording is complete, can be used only once.
  ExperimentalGroupRecorder(
      SegmentInfoDatabase* storage_service,
      FieldTrialRegister* field_trial_register,
      const std::string& segmentation_key,
      optimization_guide::proto::OptimizationTarget selected_segment);
  ~ExperimentalGroupRecorder();

  ExperimentalGroupRecorder(ExperimentalGroupRecorder&) = delete;
  ExperimentalGroupRecorder& operator=(ExperimentalGroupRecorder&) = delete;

 private:
  void OnGetSegment(absl::optional<proto::SegmentInfo> result);

  const raw_ptr<FieldTrialRegister> field_trial_register_;
  const std::string segmentation_key_;

  base::WeakPtrFactory<ExperimentalGroupRecorder> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_EXPERIMENTAL_GROUP_RECORDER_H_
