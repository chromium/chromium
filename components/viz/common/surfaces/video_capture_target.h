// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_VIDEO_CAPTURE_TARGET_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_VIDEO_CAPTURE_TARGET_H_

#include "base/token.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace viz {

// The video capture sub-target can mean one of few things:
// 1. aura::Window capture by use of SubtreeCaptureId.
// 2. Element level capture by use of SubtreeCaptureId.
// 3. Region level capture by use of a RegionCaptureCropId.
// 4. Entire tab capture (e.g. tab capture) by use of absl::monostate.
using VideoCaptureSubTarget =
    absl::variant<absl::monostate, SubtreeCaptureId, RegionCaptureCropId>;

inline bool IsEntireTabCapture(const VideoCaptureSubTarget& sub_target) {
  return absl::holds_alternative<absl::monostate>(sub_target);
}

inline bool IsSubtreeCapture(const VideoCaptureSubTarget& sub_target) {
  return absl::holds_alternative<SubtreeCaptureId>(sub_target);
}

inline bool IsRegionCapture(const VideoCaptureSubTarget& sub_target) {
  return absl::holds_alternative<RegionCaptureCropId>(sub_target);
}

// All of the information necessary to select a target for capture.
// If constructed, the |frame_sink_id| must be valid and |sub_target|
// is optional. If not provided, this target is the root render pass of
// the frame sink. If |sub_target| is a SubtreeCaptureId, this target is
// a layer subtree under the root render pass. Else, if |sub_target| is
// a RegionCaptureCropId, this target is the root render pass but only
// a subset of the pixels are selected for capture.
struct VIZ_COMMON_EXPORT VideoCaptureTarget {
  explicit VideoCaptureTarget(FrameSinkId frame_sink_id);

  // If an invalid sub target is provided, it will be internally converted to an
  // absl::monostate, equivalent to calling the single argument constructor
  // above.
  VideoCaptureTarget(FrameSinkId frame_sink_id,
                     VideoCaptureSubTarget capture_sub_target);

  VideoCaptureTarget();
  ~VideoCaptureTarget();
  VideoCaptureTarget(const VideoCaptureTarget& other);
  VideoCaptureTarget(VideoCaptureTarget&& other);
  VideoCaptureTarget& operator=(const VideoCaptureTarget& other);
  VideoCaptureTarget& operator=(VideoCaptureTarget&& other);

  friend bool operator==(const VideoCaptureTarget&,
                         const VideoCaptureTarget&) = default;

  // The target frame sink id. Must be valid.
  FrameSinkId frame_sink_id;

  // The sub target.
  VideoCaptureSubTarget sub_target;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_VIDEO_CAPTURE_TARGET_H_
