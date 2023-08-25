// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_new_tab_page_color_mixer.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "components/search/ntp_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"

namespace {

class MaterialNewTabPageColorMixerTest : public testing::Test {
 public:
  MaterialNewTabPageColorMixerTest() = default;

  ui::ColorProvider& color_provider() { return color_provider_; }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

  void AddMaterialColorMixers() {
    AddColorMixers(&color_provider_, color_provider_key_);
    AddChromeColorMixers(&color_provider_, color_provider_key_);
    color_provider_.GenerateColorMap();
  }

 private:
  ui::ColorProvider color_provider_;
  ui::ColorProviderKey color_provider_key_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MaterialNewTabPageColorMixerTest, ComprehensiveThemeRealboxEnabled) {
  feature_list().InitWithFeatures(
      /* enabled_features */ {ntp_features::kNtpComprehensiveThemeRealbox,
                              features::kChromeRefresh2023,
                              features::kChromeWebuiRefresh2023},
      /* disabled_features */ {});

  AddMaterialColorMixers();

  EXPECT_EQ(color_provider().GetColor(kColorRealboxBackground),
            color_provider().GetColor(ui::kColorSysBase));
  EXPECT_EQ(color_provider().GetColor(kColorRealboxBackgroundHovered),
            color_provider().GetColor(ui::kColorSysStateHoverOnSubtle));
  EXPECT_EQ(color_provider().GetColor(kColorRealboxForeground),
            color_provider().GetColor(ui::kColorSysOnSurfaceSubtle));
}

TEST_F(MaterialNewTabPageColorMixerTest, NtpModulesRedesignedDisabled) {
  feature_list().InitWithFeatures(
      /* enabled_features */ {features::kChromeRefresh2023,
                              features::kChromeWebuiRefresh2023},
      /* disabled_features */ {ntp_features::kNtpModulesRedesigned});

  AddMaterialColorMixers();

  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageModuleBackground),
            color_provider().GetColor(ui::kColorSysBaseContainer));
  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageModuleItemBackground),
            color_provider().GetColor(ui::kColorSysBaseContainer));
}

TEST_F(MaterialNewTabPageColorMixerTest, NtpModulesRedesignedEnabled) {
  feature_list().InitWithFeatures(
      /* enabled_features */ {ntp_features::kNtpModulesRedesigned,
                              features::kChromeRefresh2023,
                              features::kChromeWebuiRefresh2023},
      /* disabled_features */ {});

  AddMaterialColorMixers();

  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageModuleBackground),
            color_provider().GetColor(ui::kColorSysBaseContainer));
  EXPECT_EQ(color_provider().GetColor(kColorNewTabPageModuleItemBackground),
            color_provider().GetColor(ui::kColorSysSurface));
}

}  // namespace
