// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_new_tab_page_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"

namespace {

class MaterialNewTabPageColorMixerTest : public testing::Test {
 public:
  MaterialNewTabPageColorMixerTest() = default;

  ui::ColorProvider& color_provider() { return color_provider_; }

  void AddMaterialColorMixers() {
    AddColorMixers(&color_provider_, color_provider_key_);
    AddChromeColorMixers(&color_provider_, color_provider_key_);
  }

 private:
  ui::ColorProvider color_provider_;
  ui::ColorProviderKey color_provider_key_;
};

TEST_F(MaterialNewTabPageColorMixerTest, NtpModulesRedesignedEnabled) {
  AddMaterialColorMixers();

  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageModuleBackground),
            color_provider().GetColor(ui::kColorSysBaseContainer));
  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageModuleItemBackground),
            color_provider().GetColor(ui::kColorSysSurface));
}

TEST_F(MaterialNewTabPageColorMixerTest, ThreadsRailColors) {
  for (bool dark_mode : {false, true}) {
    ui::ColorProvider provider;
    ui::ColorProviderKey key;
    if (dark_mode) {
      key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
    }
    AddColorMixers(&provider, key);
    AddChromeColorMixers(&provider, key);

    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailPrimaryFont),
              provider.GetColor(ui::kColorSysOnSurface));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailSecondaryFont),
              provider.GetColor(ui::kColorSysOnSurfaceSubtle));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailBrandPrimaryFont),
              provider.GetColor(ui::kColorSysOnSurface));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailBrandSecondaryFont),
              provider.GetColor(ui::kColorSysOnSurfaceSubtle));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailBackground),
              provider.GetColor(ui::kColorSysBaseContainer));
    EXPECT_EQ(
        provider.GetColor(kColorNewTabPageThreadsRailFlyoutMenuBackground),
        provider.GetColor(ui::kColorMenuBackground));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailContainerBackground),
              provider.GetColor(ui::kColorSysBaseContainerElevated));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailSurfaceContainer),
              provider.GetColor(ui::kColorSysBaseContainerElevated));
    EXPECT_EQ(
        provider.GetColor(kColorNewTabPageThreadsRailSurfaceContainerHigh),
        provider.GetColor(ui::kColorSysSurface2));
    EXPECT_EQ(
        provider.GetColor(kColorNewTabPageThreadsRailSurfaceContainerHighest),
        provider.GetColor(ui::kColorSysStateHoverBrightBlendProtection));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailPrimary),
              provider.GetColor(ui::kColorButtonBackgroundProminent));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailTooltipContainer),
              provider.GetColor(ui::kColorPrimaryBackground));
    const SkColor hover_on_subtle =
        provider.GetColor(ui::kColorSysStateHoverOnSubtle);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailHoverBackground),
              hover_on_subtle);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailMenuOpenBackground),
              hover_on_subtle);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailSelectedBackground),
              hover_on_subtle);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailDivider),
              provider.GetColor(ui::kColorSysNeutralOutline));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailScrim),
              provider.GetColor(ui::kColorSysStateScrim));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailLoaderBase),
              provider.GetColor(dark_mode ? ui::kColorSysSurface2
                                          : ui::kColorSysSurfaceVariant));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailLoaderHighlight),
              provider.GetColor(dark_mode ? ui::kColorSysSurfaceVariant
                                          : ui::kColorSysSurface2));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailScrollbarThumb),
              provider.GetColor(ui::kColorOverlayScrollbarFill));
    EXPECT_EQ(provider.GetColor(kColorNewTabPageThreadsRailUpdateDot),
              provider.GetColor(ui::kColorSysPrimary));
  }
}

TEST_F(MaterialNewTabPageColorMixerTest, IsolatedTabPageColors) {
  for (bool dark_mode : {false, true}) {
    ui::ColorProvider provider;
    ui::ColorProviderKey key;
    if (dark_mode) {
      key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
    }
    AddColorMixers(&provider, key);
    AddChromeColorMixers(&provider, key);

    EXPECT_EQ(provider.GetColor(kColorIsolatedTabPageBackground),
              SK_ColorWHITE);
    EXPECT_EQ(provider.GetColor(kColorIsolatedTabPageCardBackground),
              SkColorSetRGB(0xF8, 0xFA, 0xFD));
    EXPECT_EQ(provider.GetColor(kColorIsolatedTabPageLink),
              SkColorSetRGB(0x0B, 0x57, 0xD0));
    EXPECT_EQ(provider.GetColor(kColorIsolatedTabPageNoticeBorder),
              SkColorSetRGB(0xA8, 0xC7, 0xFA));
    EXPECT_EQ(provider.GetColor(kColorIsolatedTabPageNoticeIcon),
              SK_ColorBLACK);
  }
}

}  // namespace
