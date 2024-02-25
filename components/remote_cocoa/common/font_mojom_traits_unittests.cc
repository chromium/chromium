// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/common/font.mojom.h"
#include "components/remote_cocoa/common/font_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/platform_font_mac.h"

TEST(FontMojomTraits, SerializeAndDeserialize) {
  std::vector<gfx::Font> fonts;
  fonts.push_back(gfx::Font());
  fonts.push_back(
      gfx::Font().Derive(-2, gfx::Font::NORMAL, gfx::Font::Weight::THIN));
  fonts.push_back(gfx::Font("Arial", 8));
  fonts.push_back(gfx::Font("Courier", 12));
  fonts.push_back(
      gfx::Font("Arial", 6)
          .Derive(4, gfx::Font::STRIKE_THROUGH, gfx::Font::Weight::NORMAL));
  fonts.push_back(gfx::Font(new gfx::PlatformFontMac(
      gfx::PlatformFontMac::SystemFontType::kGeneral)));
  fonts.push_back(gfx::Font(new gfx::PlatformFontMac(
                                gfx::PlatformFontMac::SystemFontType::kMenu))
                      .Derive(3, gfx::Font::ITALIC | gfx::Font::UNDERLINE,
                              gfx::Font::Weight::BOLD));

  for (const auto& font : fonts) {
    gfx::Font result;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<remote_cocoa::mojom::Font>(
        font, result));
    EXPECT_EQ(font.GetActualFontName(), result.GetActualFontName());
    EXPECT_EQ(font.GetFontSize(), result.GetFontSize());
    EXPECT_EQ(font.GetStyle(), result.GetStyle());
    EXPECT_EQ(font.GetWeight(), result.GetWeight());
  }
}
