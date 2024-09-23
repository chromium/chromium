// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/frame_interval_inputs.h"

namespace viz {

std::string ContentFrameIntervalTypeToString(ContentFrameIntervalType type) {
  switch (type) {
    case ContentFrameIntervalType::kVideo:
      return "video";
    case ContentFrameIntervalType::kAnimatingImage:
      return "animating_image";
    case ContentFrameIntervalType::kScrollBarFadeOutAnimation:
      return "scrollbar_fade_out";
  }
}

FrameIntervalInputs::FrameIntervalInputs() = default;
FrameIntervalInputs::FrameIntervalInputs(const FrameIntervalInputs& other) =
    default;
FrameIntervalInputs::~FrameIntervalInputs() = default;

}  // namespace viz
