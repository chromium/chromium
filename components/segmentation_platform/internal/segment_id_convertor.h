// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENT_ID_CONVERTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENT_ID_CONVERTOR_H_

#include <optional>

#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// Conversion functions between OptimizationTarget and SegmentId.
std::optional<optimization_guide::proto::OptimizationTarget>
SegmentIdToOptimizationTarget(proto::SegmentId segment_id);

// Conversion functions between OptimizationTarget and SegmentId.
proto::SegmentId OptimizationTargetToSegmentId(
    optimization_guide::proto::OptimizationTarget segment_id);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SEGMENT_ID_CONVERTOR_H_
