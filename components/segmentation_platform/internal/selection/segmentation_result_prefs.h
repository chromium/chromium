// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENTATION_RESULT_PREFS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENTATION_RESULT_PREFS_H_

#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

// Struct containing information about the selected segment. Convenient for
// reading and writing to prefs.
struct SelectedSegment {
 public:
  OptimizationTarget segment_id;
  base::Time selection_time;
  bool in_use;

  explicit SelectedSegment(OptimizationTarget segment_id);
};

// Stores the result of segmentation into prefs for faster lookup. The result
// consists of (1) The selected segment ID. (2) The time when the segment was
// first selected. Used to enforce segment selection TTL. (3) Whether the
// selected segment has started to be used by clients.
class SegmentationResultPrefs {
 public:
  virtual ~SegmentationResultPrefs() = default;

  // Writes the selected segment to prefs. Deletes the previous results if
  // |selected_segment| is empty.
  virtual void SaveSegmentationResultToPref(
      const absl::optional<SelectedSegment>& selected_segment) = 0;

  // Reads the selected segment from pref, if any.
  virtual absl::optional<SelectedSegment> ReadSegmentationResultFromPref() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENTATION_RESULT_PREFS_H_
