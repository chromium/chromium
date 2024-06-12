// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"

#include <algorithm>

#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace video_format_comparison {

namespace {

constexpr float kAspectRatioFitnessWeight = 100;
constexpr float kWidthFitnessWeight = 20;
constexpr float kFrameRateFitnessWeight = 20;
constexpr float kPunishmentDiscount = 0.09;

// Return fitness value for aspect ratio in the range [0, 1].
// The closer the aspect ratio value to `kDefaultAspectRatio`, the higher the
// fitness value returned.
float GetAspectRatioFitness(const media::VideoCaptureFormat& format) {
  const float distance_from_default =
      std::fabs(GetFrameAspectRatio(format.frame_size) - kDefaultAspectRatio);
  return std::fmax(1 - distance_from_default, 0);
}

// Return fitness value in the range [0, 1].
// The closer `value` is to `target`, the higher the fitness value returned. But
// we still favor values that are above `target` rather than values that are
// below it. For example, a `value of 60` has higher fitness than a `value of
// 24` when `target is 30`.
float GetFitnessValue(const float value, const float target) {
  if (value > target) {
    const float fitness = 1 - value / target * kPunishmentDiscount;
    return std::fmax(fitness, 0);
  } else {
    return value / target;
  }
}

// Return the format fitness value. The higher the value returned the more fit
// `format` is. Lowest value is 0.
float GetFormatFitness(const media::VideoCaptureFormat& format,
                       const int view_width,
                       const float target_frame_rate) {
  float aspect_ratio_fitness =
      GetAspectRatioFitness(format) * kAspectRatioFitnessWeight;
  float width_fitness = GetFitnessValue(format.frame_size.width(), view_width) *
                        kWidthFitnessWeight;
  float frame_rate_fitness =
      GetFitnessValue(format.frame_rate, target_frame_rate) *
      kFrameRateFitnessWeight;

  return aspect_ratio_fitness + width_fitness + frame_rate_fitness;
}

}  // namespace

float GetFrameAspectRatio(const gfx::Size& frame_size) {
  return frame_size.width() / static_cast<float>(frame_size.height());
}

media::VideoCaptureFormat GetClosestVideoFormat(
    const std::vector<media::VideoCaptureFormat>& formats,
    const int view_width,
    const float target_frame_rate) {
  float chosen_fitness = 0;
  media::VideoCaptureFormat chosen_format = media::VideoCaptureFormat();
  for (const auto& format : formats) {
    const float fitness =
        GetFormatFitness(format, view_width, target_frame_rate);
    if (fitness > chosen_fitness) {
      chosen_fitness = fitness;
      chosen_format = format;
    }
  }
  return chosen_format;
}

}  // namespace video_format_comparison
