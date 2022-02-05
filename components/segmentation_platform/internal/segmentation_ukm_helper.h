// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

// A helper class to record segmentation model execution results in UKM.
class SegmentationUkmHelper {
 public:
  static SegmentationUkmHelper* GetInstance();
  SegmentationUkmHelper(const SegmentationUkmHelper&) = delete;
  SegmentationUkmHelper& operator=(const SegmentationUkmHelper&) = delete;

  // Record segmentation model information and input/output after the
  // executing the model, and return the UKM source ID.
  ukm::SourceId RecordModelExecutionResult(
      OptimizationTarget segment_id,
      int64_t model_version,
      const std::vector<float>& input_tensor,
      float result);

  // Helper method to encode a float number into int64.
  static int64_t FloatToInt64(float f);

 private:
  friend class base::NoDestructor<SegmentationUkmHelper>;
  friend class SegmentationUkmHelperTest;
  SegmentationUkmHelper();
  ~SegmentationUkmHelper();

  void Initialize();

  base::flat_set<int> allowed_segment_ids_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENTATION_UKM_HELPER_H_
