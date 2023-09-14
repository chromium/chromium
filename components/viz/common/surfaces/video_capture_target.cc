// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/video_capture_target.h"

#include "base/logging.h"

namespace viz {

namespace {
bool SubTargetIsValid(const VideoCaptureSubTarget& sub_target) {
  if (IsEntireTabCapture(sub_target)) {
    return true;
  }
  if (const auto* crop_id = absl::get_if<RegionCaptureCropId>(&sub_target)) {
    return !crop_id->is_zero();
  }
  const auto* capture_id = absl::get_if<SubtreeCaptureId>(&sub_target);
  DCHECK(capture_id);
  return capture_id->is_valid();
}

}  // namespace

VideoCaptureTarget::VideoCaptureTarget(FrameSinkId frame_sink_id)
    : VideoCaptureTarget(frame_sink_id, VideoCaptureSubTarget()) {}
VideoCaptureTarget::VideoCaptureTarget(FrameSinkId frame_sink_id,
                                       VideoCaptureSubTarget sub_target)
    : frame_sink_id(frame_sink_id),
      sub_target(SubTargetIsValid(sub_target) ? sub_target
                                              : VideoCaptureSubTarget{}) {
  DCHECK(this->frame_sink_id.is_valid());
}

VideoCaptureTarget::VideoCaptureTarget() = default;
VideoCaptureTarget::VideoCaptureTarget(const VideoCaptureTarget& other) =
    default;
VideoCaptureTarget::VideoCaptureTarget(VideoCaptureTarget&& other) = default;
VideoCaptureTarget& VideoCaptureTarget::operator=(
    const VideoCaptureTarget& other) = default;
VideoCaptureTarget& VideoCaptureTarget::operator=(VideoCaptureTarget&& other) =
    default;

VideoCaptureTarget::~VideoCaptureTarget() = default;

}  // namespace viz
