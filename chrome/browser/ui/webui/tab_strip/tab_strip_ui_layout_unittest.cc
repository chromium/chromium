// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"

#include <algorithm>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

// The test parameter is the web content's viewport size.
class TabStripUILayoutAspectRatioTest
    : public ::testing::TestWithParam<gfx::Size> {};

TEST_P(TabStripUILayoutAspectRatioTest, ThumbnailHasSameAspectRatioAsViewport) {
  const gfx::Size viewport_size = GetParam();
  TabStripUILayout layout =
      TabStripUILayout::CalculateForWebViewportSize(viewport_size);

  EXPECT_EQ(120, layout.tab_thumbnail_size.height());
  EXPECT_FLOAT_EQ(
      std::max(1.0, 1.0 * viewport_size.width() / viewport_size.height()),
      1.0 * layout.tab_thumbnail_aspect_ratio);
}

INSTANTIATE_TEST_SUITE_P(SmallSizes,
                         TabStripUILayoutAspectRatioTest,
                         ::testing::Values(gfx::Size(200, 200),
                                           gfx::Size(200, 300),
                                           gfx::Size(300, 200)));

INSTANTIATE_TEST_SUITE_P(LargeSizes,
                         TabStripUILayoutAspectRatioTest,
                         ::testing::Values(gfx::Size(1920, 1080),
                                           gfx::Size(1080, 1920)));

TEST(TabStripUILayoutTest, HandlesZeroSize) {
  TabStripUILayout layout =
      TabStripUILayout::CalculateForWebViewportSize(gfx::Size(0, 0));
  EXPECT_FALSE(layout.tab_thumbnail_size.IsEmpty());
}
