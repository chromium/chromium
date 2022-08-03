// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METRIC_FILTER_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METRIC_FILTER_UTILS_H_

#include <array>
#include <string>
#include <vector>

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform::stats {

// Returns a name to be used in UMA dashboard as segment group for the given
// `segment_id`.
std::string OptimizationTargetToSegmentGroupName(proto::SegmentId segment_id);

// Returns a name to be used in UMA dashboard as segmentation type for the given
// `segmentation_key`.
std::string SegmentationKeyToTrialName(const std::string& segmentation_key);

// Returns a name to be used in UMA dashboard as segmentation subtype type for
// the given `segmentation_key` and `segment_id`.
std::string SegmentationKeyToSubsegmentTrialName(
    const std::string& segmentation_key,
    proto::SegmentId segment_id);

}  // namespace segmentation_platform::stats

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METRIC_FILTER_UTILS_H_
