// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_color_provider_utils.h"

#include "base/test/gtest_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"

using ChromeColorProviderUtilsTest = ::testing::Test;

TEST_F(ChromeColorProviderUtilsTest, ChromeColorIdNameTest) {
  EXPECT_EQ("kColorToolbarBackgroundSubtleEmphasis",
            ChromeColorIdName(kColorToolbarBackgroundSubtleEmphasis));
  ui::ColorProvider provider;
  AddChromeColorMixers(&provider, ui::ColorProviderKey());
  EXPECT_EQ("kColorToolbarBackgroundSubtleEmphasis",
            ui::ColorIdName(kColorToolbarBackgroundSubtleEmphasis));
}

TEST_F(ChromeColorProviderUtilsTest, ChromeColorIdToCSSColorIdTest) {
  EXPECT_EQ("--color-toolbar-background-subtle-emphasis",
            ChromeColorIdToCSSColorId(kColorToolbarBackgroundSubtleEmphasis));
  EXPECT_EQ(ui::ConvertColorProviderColorIdToCSSColorId(
                ChromeColorIdName(kChromeColorsStart)),
            ChromeColorIdToCSSColorId(kChromeColorsStart));
  EXPECT_EQ(ui::ConvertColorProviderColorIdToCSSColorId(
                ChromeColorIdName(kChromeColorsEnd - 1)),
            ChromeColorIdToCSSColorId(kChromeColorsEnd - 1));
  EXPECT_TRUE(ChromeColorIdToCSSColorId(kChromeColorsStart - 1).empty());
  EXPECT_TRUE(ChromeColorIdToCSSColorId(kChromeColorsEnd).empty());
}
