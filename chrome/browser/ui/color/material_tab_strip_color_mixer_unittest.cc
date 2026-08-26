// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_tab_strip_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"

namespace {

TEST(MaterialTabStripColorMixerTest, DefaultFrameStyleDoesNotOverride) {
  ui::ColorProvider provider;
  ui::ColorProviderKey key;
  key.frame_style = ui::ColorProviderKey::FrameStyle::kDefault;

  AddChromeColorMixers(&provider, key);

  // In standard non-glass frame, inactive tab background is not transparent.
  EXPECT_NE(provider.GetColor(kColorTabBackgroundInactiveFrameActive),
            SK_ColorTRANSPARENT);
}

TEST(MaterialTabStripColorMixerTest, GlassFrameStyleOverridesColors) {
  ui::ColorProvider provider;
  ui::ColorProviderKey key;
  key.frame_style = ui::ColorProviderKey::FrameStyle::kGlass;

  AddChromeColorMixers(&provider, key);

  // In glass frame, selected tab background has 80% opacity.
  constexpr SkAlpha kExpectedAlpha = 0.80 * SK_AlphaOPAQUE;
  EXPECT_EQ(
      SkColorGetA(provider.GetColor(kColorTabBackgroundSelectedFrameActive)),
      kExpectedAlpha);
  EXPECT_EQ(
      SkColorGetA(provider.GetColor(kColorTabBackgroundSelectedFrameInactive)),
      kExpectedAlpha);

  // In glass frame, new tab button backgrounds are transparent.
  EXPECT_EQ(provider.GetColor(kColorNewTabButtonBackgroundFrameActive),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(provider.GetColor(kColorNewTabButtonBackgroundFrameInactive),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(provider.GetColor(kColorNewTabButtonCRBackgroundFrameActive),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(provider.GetColor(kColorNewTabButtonCRBackgroundFrameInactive),
            SK_ColorTRANSPARENT);
}

}  // namespace
