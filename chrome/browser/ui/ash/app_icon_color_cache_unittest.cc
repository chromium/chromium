// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/ash/app_icon_color_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"

using AppIconColorCacheTestTest = testing::Test;

namespace ash {

TEST_F(AppIconColorCacheTestTest, ExtractedLightVibrantColorTest) {
  const int width = 64;
  const int height = 64;

  SkBitmap all_black_icon;
  all_black_icon.allocN32Pixels(width, height);
  all_black_icon.eraseColor(SK_ColorBLACK);

  SkColor test_color =
      AppIconColorCache::GetInstance().GetLightVibrantColorForApp(
          "app_id1", gfx::ImageSkia::CreateFrom1xBitmap(all_black_icon));

  // For an all black icon, a default white color is expected, since there
  // is no other light vibrant color to get from the icon.
  EXPECT_EQ(test_color, SK_ColorWHITE);

  // Create an icon that is half kGoogleRed300 and half kGoogleRed600.
  SkBitmap red_icon;
  red_icon.allocN32Pixels(width, height);
  red_icon.eraseColor(gfx::kGoogleRed300);
  red_icon.erase(gfx::kGoogleRed600, {0, 0, width, height / 2});

  test_color = AppIconColorCache::GetInstance().GetLightVibrantColorForApp(
      "app_id2", gfx::ImageSkia::CreateFrom1xBitmap(red_icon));

  // For the red icon, the color cache should calculate and use the
  // kGoogleRed300 color as the light vibrant color taken from the icon.
  EXPECT_EQ(gfx::kGoogleRed300, test_color);
}

}  // namespace ash
