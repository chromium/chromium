// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform {

SegmentSelectionResult::SegmentSelectionResult() = default;

SegmentSelectionResult::~SegmentSelectionResult() = default;

SegmentSelectionResult::SegmentSelectionResult(
    const SegmentSelectionResult& other) = default;

SegmentSelectionResult& SegmentSelectionResult::operator=(
    const SegmentSelectionResult& other) = default;

bool SegmentSelectionResult::operator==(
    const SegmentSelectionResult& other) const {
  return is_ready == other.is_ready && segment == other.segment;
}

}  // namespace segmentation_platform
