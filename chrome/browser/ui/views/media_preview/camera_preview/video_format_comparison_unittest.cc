// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"

#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_format_comparison {

TEST(VideoFormatComparisonTest, ChooseTheClosetFormat) {
  const std::vector<media::VideoCaptureFormat> formats = {
      {{160, 120}, 15.0, media::PIXEL_FORMAT_I420},
      {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
      {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
      {{640, 480}, 30.0, media::PIXEL_FORMAT_I420},
      {{3840, 2160}, 30.0, media::PIXEL_FORMAT_Y16},
      {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
      {{1280, 720}, 30.0, media::PIXEL_FORMAT_I420}};

  EXPECT_EQ(formats[0], GetClosestVideoFormat(formats, /*view_width=*/130,
                                              /*minimum_frame_rate*/ 10));
  EXPECT_EQ(formats[3], GetClosestVideoFormat(formats, /*view_width=*/300,
                                              /*minimum_frame_rate*/ 10));

  EXPECT_EQ(formats[3], GetClosestVideoFormat(formats, /*view_width=*/280,
                                              /*minimum_frame_rate*/ 30));
  EXPECT_EQ(formats[5], GetClosestVideoFormat(formats, /*view_width=*/700,
                                              /*minimum_frame_rate*/ 30));

  EXPECT_EQ(formats[3], GetClosestVideoFormat(formats, /*view_width=*/280,
                                              /*minimum_frame_rate*/ 40));
}

}  // namespace video_format_comparison
