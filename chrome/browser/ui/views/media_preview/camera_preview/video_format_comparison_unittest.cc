// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_format_comparison.h"

#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_format_comparison {

TEST(VideoFormatComparisonTest, FrameRateTest) {
  // Not relevant in this test format comparison.
  const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  const int view_width = 400;
  const float target_frame_rate = 30;

  const media::VideoCaptureFormat f1 = {{844, 400}, 24.0, pixel_format};
  const media::VideoCaptureFormat f2 = {{844, 400}, 60.0, pixel_format};
  EXPECT_EQ(f2, GetClosestVideoFormat({f1, f2}, view_width, target_frame_rate));
  EXPECT_EQ(f2, GetClosestVideoFormat({f2, f1}, view_width, target_frame_rate));

  const media::VideoCaptureFormat f3 = {{1280, 720}, 24.0, pixel_format};
  const media::VideoCaptureFormat f4 = {{1280, 720}, 60.0, pixel_format};
  EXPECT_EQ(f4, GetClosestVideoFormat({f3, f4}, view_width, target_frame_rate));
  EXPECT_EQ(f4, GetClosestVideoFormat({f4, f3}, view_width, target_frame_rate));

  const media::VideoCaptureFormat f5 = {{1280, 720}, 26.0, pixel_format};
  EXPECT_EQ(f5, GetClosestVideoFormat({f4, f5}, view_width, target_frame_rate));
  EXPECT_EQ(f5, GetClosestVideoFormat({f5, f4}, view_width, target_frame_rate));
}

TEST(VideoFormatComparisonTest, FrameRateTest2) {
  // Not relevant in this test format comparison.
  const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  const int view_width = 400;
  const float target_frame_rate = 30;

  const media::VideoCaptureFormat f1 = {{844, 400}, 24.0, pixel_format};
  const media::VideoCaptureFormat f2 = {{844, 400}, 120.0, pixel_format};
  EXPECT_EQ(f1, GetClosestVideoFormat({f1, f2}, view_width, target_frame_rate));
  EXPECT_EQ(f1, GetClosestVideoFormat({f2, f1}, view_width, target_frame_rate));
}

TEST(VideoFormatComparisonTest, WidthViewTest) {
  // Not relevant in this test format comparison.
  const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  const float target_frame_rate = 30;

  int view_width = 400;
  const media::VideoCaptureFormat f1 = {{1280, 720}, 30.0, pixel_format};
  const media::VideoCaptureFormat f2 = {{640, 360}, 30.0, pixel_format};
  EXPECT_EQ(f2, GetClosestVideoFormat({f1, f2}, view_width, target_frame_rate));
  EXPECT_EQ(f2, GetClosestVideoFormat({f2, f1}, view_width, target_frame_rate));

  view_width = 800;
  const media::VideoCaptureFormat f3 = {{1280, 720}, 60.0, pixel_format};
  const media::VideoCaptureFormat f4 = {{640, 360}, 60.0, pixel_format};
  EXPECT_EQ(f3, GetClosestVideoFormat({f3, f4}, view_width, target_frame_rate));
  EXPECT_EQ(f3, GetClosestVideoFormat({f4, f3}, view_width, target_frame_rate));
}

TEST(VideoFormatComparisonTest, ChooseTheClosetFormat) {
  const std::vector<media::VideoCaptureFormat> formats = {
      {{160, 120}, 15.0, media::PIXEL_FORMAT_I420},
      {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
      {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
      {{640, 480}, 30.0, media::PIXEL_FORMAT_I420},
      {{3840, 2160}, 30.0, media::PIXEL_FORMAT_Y16},
      {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
      {{1280, 720}, 30.0, media::PIXEL_FORMAT_I420}};

  // Aspect ratio is the main decider here (it has the most weight).

  EXPECT_EQ(formats[6], GetClosestVideoFormat(formats, /*view_width=*/130,
                                              /*target_frame_rate=*/10));
  EXPECT_EQ(formats[6], GetClosestVideoFormat(formats, /*view_width=*/300,
                                              /*target_frame_rate=*/10));

  EXPECT_EQ(formats[6], GetClosestVideoFormat(formats, /*view_width=*/280,
                                              /*target_frame_rate=*/30));
  EXPECT_EQ(formats[6], GetClosestVideoFormat(formats, /*view_width=*/700,
                                              /*target_frame_rate=*/30));

  EXPECT_EQ(formats[6], GetClosestVideoFormat(formats, /*view_width=*/280,
                                              /*target_frame_rate=*/40));
}

}  // namespace video_format_comparison
