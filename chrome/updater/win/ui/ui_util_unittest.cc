// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/ui_util.h"

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace updater::ui {

TEST(UiUtilTest, IsColorDark) {
  // Pure black and white.
  EXPECT_TRUE(IsColorDark(RGB(0, 0, 0)));
  EXPECT_FALSE(IsColorDark(RGB(255, 255, 255)));

  // Midpoint gray boundaries.
  EXPECT_TRUE(IsColorDark(RGB(127, 127, 127)));
  EXPECT_FALSE(IsColorDark(RGB(128, 128, 128)));

  // Windows High Contrast Themes window background colors.
  // High Contrast Black / Night Sky (Dark)
  EXPECT_TRUE(IsColorDark(RGB(0, 0, 0)));
  // High Contrast White (Light)
  EXPECT_FALSE(IsColorDark(RGB(255, 255, 255)));
  // Aquatic theme (Dark blue/teal background)
  EXPECT_TRUE(IsColorDark(RGB(32, 32, 32)));
  EXPECT_TRUE(IsColorDark(RGB(0, 32, 48)));
  // Desert theme (Light cream/beige background)
  EXPECT_FALSE(IsColorDark(RGB(255, 250, 239)));
}

}  // namespace updater::ui
