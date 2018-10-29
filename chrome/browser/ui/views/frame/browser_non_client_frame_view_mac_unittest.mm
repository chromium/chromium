// Copyright 2018 The Chromium Authors. All rights reserved.
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
