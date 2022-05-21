// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segment_id_convertor.h"

namespace segmentation_platform {

optimization_guide::proto::OptimizationTarget SegmentIdToOptimizationTarget(
    proto::SegmentId segment_id) {
  return static_cast<optimization_guide::proto::OptimizationTarget>(segment_id);
}

proto::SegmentId OptimizationTargetToSegmentId(
    optimization_guide::proto::OptimizationTarget segment_id) {
  return static_cast<proto::SegmentId>(segment_id);
}

}  // namespace segmentation_platform
