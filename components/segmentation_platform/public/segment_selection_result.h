// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENT_SELECTION_RESULT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENT_SELECTION_RESULT_H_

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  absl::optional<proto::SegmentId> segment;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENT_SELECTION_RESULT_H_
