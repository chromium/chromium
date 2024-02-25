// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"

#include <algorithm>

#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace video_format_comparison {

namespace {

bool IsAcceptableFormat(const media::VideoCaptureFormat& format,
                        const float minimum_frame_rate,
                        const int view_width) {
  return format.frame_rate >= minimum_frame_rate &&
         format.frame_size.width() >= view_width &&
         GetFrameAspectRatio(format.frame_size) >= kMinAspectRatio;
}

// Returns true if `other` value is better than `cur` value. If both values are
// larger than or equal the `target`, then the lower among the two
// values would be more suitable. On the other hand, if one or both the values
// are less than `target`, then the higher among the two values would
// be more suitable.
bool IsBetterValue(const float cur, const float other, const float target) {
  float preferred_value = std::min(cur, other);
  if (cur < target || other < target) {
    preferred_value = std::max(cur, other);
  }
  return other == preferred_value;
}

// Returns true if `other` format is better than `cur` format.
// Better here means: (1) If one format is acceptable and the other is not, then
// the acceptable is better. (2) If both formats are acceptable or both are
// unacceptable, then check their frame rate, width, and aspect ratio in that
// order, and decide which suits more.
// For suits more definition, check `IsBetterValue(..)`.
bool IsBetterFormat(const media::VideoCaptureFormat& cur,
                    const media::VideoCaptureFormat& other,
                    const int view_width,
                    const float minimum_frame_rate) {
  const bool is_other_acceptable =
      IsAcceptableFormat(other, minimum_frame_rate, view_width);
  if (is_other_acceptable !=
      IsAcceptableFormat(cur, minimum_frame_rate, view_width)) {
    return is_other_acceptable;
  }

  if (cur.frame_rate != other.frame_rate) {
    return IsBetterValue(cur.frame_rate, other.frame_rate, minimum_frame_rate);
  }
  if (cur.frame_size.width() != other.frame_size.width()) {
    return IsBetterValue(cur.frame_size.width(), other.frame_size.width(),
                         view_width);
  }
  return IsBetterValue(GetFrameAspectRatio(cur.frame_size),
                       GetFrameAspectRatio(other.frame_size),
                       kDefaultAspectRatio);
}

}  // namespace

float GetFrameAspectRatio(const gfx::Size& frame_size) {
  return frame_size.width() / static_cast<float>(frame_size.height());
}

media::VideoCaptureFormat GetClosestVideoFormat(
    const std::vector<media::VideoCaptureFormat>& formats,
    const int view_width,
    const float target_frame_rate) {
  const media::VideoCaptureFormat* chosen_format = nullptr;
  for (const auto& format : formats) {
    if (!chosen_format ||
        IsBetterFormat(*chosen_format, format, view_width, target_frame_rate)) {
      chosen_format = &format;
    }
  }
  return chosen_format ? *chosen_format : media::VideoCaptureFormat();
}

}  // namespace video_format_comparison
