// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/metric_filter_utils.h"

#include "base/strings/strcat.h"
#include "components/segmentation_platform/internal/stats.h"

namespace segmentation_platform::stats {
namespace {
using proto::SegmentId;

}  // namespace

std::string OptimizationTargetToSegmentGroupName(SegmentId segment_id) {
  return OptimizationTargetToHistogramVariant(segment_id);
}

std::string SegmentationKeyToTrialName(const std::string& segmentation_key) {
  return base::StrCat(
      {"Segmentation_", SegmentationKeyToUmaName(segmentation_key)});
}

std::string SegmentationKeyToSubsegmentTrialName(
    const std::string& segmentation_key,
    proto::SegmentId segment_id) {
  return base::StrCat({"Segmentation_",
                       SegmentationKeyToUmaName(segmentation_key), "_",
                       OptimizationTargetToHistogramVariant(segment_id)});
}

}  // namespace segmentation_platform::stats
