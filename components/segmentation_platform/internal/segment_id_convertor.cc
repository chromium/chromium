// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/segment_id_convertor.h"

#include "base/check_op.h"

namespace segmentation_platform {

std::optional<optimization_guide::proto::OptimizationTarget>
SegmentIdToOptimizationTarget(proto::SegmentId segment_id) {
  DCHECK_LT(static_cast<int>(optimization_guide::proto::OptimizationTarget_MAX),
            static_cast<int>(proto::SegmentId::MAX_OPTIMIZATION_TARGET));
  if (static_cast<int>(segment_id) >=
      static_cast<int>(proto::SegmentId::MAX_OPTIMIZATION_TARGET)) {
    return std::nullopt;
  }
  return static_cast<optimization_guide::proto::OptimizationTarget>(segment_id);
}

proto::SegmentId OptimizationTargetToSegmentId(
    optimization_guide::proto::OptimizationTarget segment_id) {
  return static_cast<proto::SegmentId>(segment_id);
}

}  // namespace segmentation_platform
