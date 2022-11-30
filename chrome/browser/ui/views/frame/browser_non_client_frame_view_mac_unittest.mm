// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BrowserNonClientFrameViewMacTest, GetCenteredTitleBounds) {
  struct {
    int frame_width;
    int frame_height;
    int left_inset_x;
    int right_inset_x;
    int title_width;
    int expected_title_x;
    int expected_title_width;
  } test_cases[] = {
      {800, 40, 0, 800, 400, 200, 400},   {800, 40, 100, 700, 400, 200, 400},
      {800, 40, 250, 550, 400, 250, 300}, {800, 40, 350, 450, 400, 350, 100},
      {800, 40, 100, 700, 400, 200, 400}, {800, 40, 250, 700, 400, 250, 400},
      {800, 40, 350, 700, 400, 350, 350}, {800, 40, 650, 700, 400, 650, 50},
      {800, 40, 100, 700, 400, 200, 400}, {800, 40, 100, 550, 400, 150, 400},
      {800, 40, 100, 450, 400, 100, 350}, {800, 40, 100, 150, 400, 100, 50},
  };

  int index = 0;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("\nTest case index: %d", index));
    gfx::Rect title_bounds =
        BrowserNonClientFrameViewMac::GetCenteredTitleBounds(
            test_case.frame_width, test_case.frame_height,
            test_case.left_inset_x, test_case.right_inset_x,
            test_case.title_width);
    gfx::Rect expected_title_bounds =
        gfx::Rect(test_case.expected_title_x, 0, test_case.expected_title_width,
                  test_case.frame_height);
    EXPECT_EQ(title_bounds, expected_title_bounds);
    index++;
  }
}

TEST(BrowserNonClientFrameViewMacTest, GetCaptionButtonPlaceholderBounds) {
  const gfx::Size frame(800, 40);
  const int width = 85;  // 75 + 10 (padding)
  const int y = 0;

  const gfx::Rect ltr_bounds =
      BrowserNonClientFrameViewMac::GetCaptionButtonPlaceholderBounds(
          false /* is_rtl */, frame, y, width);
  const gfx::Rect expected_ltr_bounds = gfx::Rect(0, 0, 85, 40);

  EXPECT_EQ(ltr_bounds, expected_ltr_bounds);

  const gfx::Rect rtl_bounds =
      BrowserNonClientFrameViewMac::GetCaptionButtonPlaceholderBounds(
          true /* is_rtl */, frame, y, width);
  const gfx::Rect expected_rtl_bounds = gfx::Rect(715, 0, 85, 40);

  EXPECT_EQ(rtl_bounds, expected_rtl_bounds);
}

TEST(BrowserNonClientFrameViewMacTest, GetWebAppFrameToolbarAvailableBounds) {
  const gfx::Size frame(800, 40);
  const int y = 0;
  const int caption_button_container_width = 75;

  const gfx::Rect ltr_available_bounds =
      BrowserNonClientFrameViewMac::GetWebAppFrameToolbarAvailableBounds(
          false /* is_rtl */, frame, y, caption_button_container_width);
  const gfx::Rect expected_ltr_available_bounds =
      gfx::Rect(caption_button_container_width, y,
                frame.width() - caption_button_container_width, frame.height());

  EXPECT_EQ(ltr_available_bounds, expected_ltr_available_bounds);

  const gfx::Rect rtl_available_bounds =
      BrowserNonClientFrameViewMac::GetWebAppFrameToolbarAvailableBounds(
          true /* is_rtl */, frame, y, caption_button_container_width);
  const gfx::Rect expected_rtl_available_bounds = gfx::Rect(
      0, y, frame.width() - caption_button_container_width, frame.height());

  EXPECT_EQ(rtl_available_bounds, expected_rtl_available_bounds);
}
