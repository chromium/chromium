// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_FORMAT_COMPARISON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_FORMAT_COMPARISON_H_

#include <vector>

namespace gfx {
class Size;
}  // namespace gfx

namespace media {
struct VideoCaptureFormat;
}  // namespace media

namespace video_format_comparison {

inline constexpr float kDefaultFrameRate = 30.0f;
inline constexpr float kDefaultAspectRatio = 16.0 / 9.0;

float GetFrameAspectRatio(const gfx::Size& frame_size);

// Given a list of supported formats, return the least taxing acceptable format
// if exist. If no acceptable format exist, then return the closest that exist.
media::VideoCaptureFormat GetClosestVideoFormat(
    const std::vector<media::VideoCaptureFormat>& formats,
    const int view_width,
    const float target_frame_rate = kDefaultFrameRate);

}  // namespace video_format_comparison

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_VIDEO_FORMAT_COMPARISON_H_
