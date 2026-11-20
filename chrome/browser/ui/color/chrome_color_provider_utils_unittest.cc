// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_provider_utils.h"

#include <optional>

#include "base/test/gtest_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_utils.h"

namespace {

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

TEST_F(ChromeColorProviderUtilsTest, ToolbarFocusRingBlendsForMinContrast) {
  EXPECT_EQ("--color-toolbar-focus-ring",
            ChromeColorIdToCSSColorId(kColorToolbarFocusRing));

  ui::ColorProviderKey key;
  ui::ColorProvider provider;
  ui::AddColorMixers(&provider, key);
  AddChromeColorMixers(&provider, key);

  // Default theme already meets 3:1 contrast against kColorToolbar.
  EXPECT_EQ(provider.GetColor(ui::kColorFocusableBorderFocused),
            provider.GetColor(kColorToolbarFocusRing));

  // When kColorFocusableBorderFocused and kColorToolbar have < 3:1 contrast,
  // kColorToolbarFocusRing blends toward max contrast to reach 3:1.
  ui::ColorMixer& custom_mixer = provider.AddMixer();
  custom_mixer[kColorToolbar] = {SK_ColorBLUE};
  custom_mixer[ui::kColorFocusableBorderFocused] = {SK_ColorBLUE};
  const SkColor expected = color_utils::BlendForMinContrast(
                               SK_ColorBLUE, SK_ColorBLUE, std::nullopt,
                               color_utils::kMinimumVisibleContrastRatio)
                               .color;
  EXPECT_EQ(expected, provider.GetColor(kColorToolbarFocusRing));
  EXPECT_GE(color_utils::GetContrastRatio(
                provider.GetColor(kColorToolbarFocusRing), SK_ColorBLUE),
            color_utils::kMinimumVisibleContrastRatio);
}

}  // namespace
