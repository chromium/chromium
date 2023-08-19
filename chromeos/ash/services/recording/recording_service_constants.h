// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_CONSTANTS_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/color_space.h"

namespace recording {

// The maximum FPS the video recording is captured at.
constexpr int kMaxFrameRate = 30;

// Based on the above FPS, this is the minimum duration between any two frames.
constexpr base::TimeDelta kMinCapturePeriod = base::Hertz(kMaxFrameRate);

// The minimum amount of time that must pass between any two successive size
// changes of video frames. This is needed to avoid producing a lot of video
// frames with different sizes (e.g. when resizing a window) which can result in
// a large output.
constexpr base::TimeDelta kMinPeriodForResizeThrottling =
    base::Milliseconds(500);

// The color space used for video capturing and encoding.
constexpr gfx::ColorSpace kColorSpace = gfx::ColorSpace::CreateREC709();

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_CONSTANTS_H_
