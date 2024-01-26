// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENT_SELECTION_RESULT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENT_SELECTION_RESULT_H_

#include <optional>

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// The result of segmentation and related metadata.
struct SegmentSelectionResult {
  SegmentSelectionResult();
  ~SegmentSelectionResult();
  SegmentSelectionResult(const SegmentSelectionResult& other);
  SegmentSelectionResult& operator=(const SegmentSelectionResult& other);
  bool operator==(const SegmentSelectionResult& other) const;

  // Whether or not the segmentation backend is ready, and has enough data for
  // computing a segment.
  bool is_ready{false};

  // The result of segmentation. Can be empty if the the backend couldn't select
  // a segment with confidence.
  std::optional<proto::SegmentId> segment;

  // The discrete score computed based on the `segment` model execution. If a
  // discrete mapping is not provided, the value will be equal to the model
  // score. Otherwise the value will be the mapped score based on the mapping.
  std::optional<float> rank;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENT_SELECTION_RESULT_H_
