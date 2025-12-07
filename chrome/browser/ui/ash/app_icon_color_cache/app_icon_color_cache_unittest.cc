// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_icon_color_cache/app_icon_color_cache.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class AppIconColorCacheTestTest
    : public testing::Test,
      public testing::WithParamInterface</*enable_icon_color_cache=*/bool> {
 public:
  Profile* profile() { return &profile_; }

 private:
  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        kEnablePersistentAshIconColorCache, GetParam());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppIconColorCacheTestTest,
                         /*enable_icon_color_cache=*/testing::Bool());

TEST_P(AppIconColorCacheTestTest, ExtractedLightVibrantColorTest) {
  constexpr int width = 64;
  constexpr int height = 64;

  SkBitmap all_black_icon;
  all_black_icon.allocN32Pixels(width, height);
  all_black_icon.eraseColor(SK_ColorBLACK);

  SkColor test_color =
      AppIconColorCache::GetInstance(profile()).GetLightVibrantColorForApp(
          "app_id1", gfx::ImageSkia::CreateFrom1xBitmap(all_black_icon));

  // For an all black icon, a default white color is expected, since there
  // is no other light vibrant color to get from the icon.
  EXPECT_EQ(test_color, SK_ColorWHITE);

  // Create an icon that is half kGoogleRed300 and half kGoogleRed600.
  SkBitmap red_icon;
  red_icon.allocN32Pixels(width, height);
  red_icon.eraseColor(gfx::kGoogleRed300);
  red_icon.erase(gfx::kGoogleRed600, {0, 0, width, height / 2});

  test_color =
      AppIconColorCache::GetInstance(profile()).GetLightVibrantColorForApp(
          "app_id2", gfx::ImageSkia::CreateFrom1xBitmap(red_icon));

  // For the red icon, the color cache should calculate and use the
  // kGoogleRed300 color as the light vibrant color taken from the icon.
  EXPECT_EQ(gfx::kGoogleRed300, test_color);
}

gfx::ImageSkia GetRedIconWithBackgroundColorOf(SkColor color) {
  const int width = 64;
  const int height = 64;

  SkBitmap icon;
  icon.allocN32Pixels(width, height);
  icon.eraseColor(color);

  icon.erase(gfx::kGoogleRed300,
             {width / 4, height / 4, width / 2, height / 2});

  return gfx::ImageSkia::CreateFrom1xBitmap(icon);
}

TEST_P(AppIconColorCacheTestTest, SortableIconColorTest) {
  const int red_hue = 49;

  // Test a red icon with a black background color.
  IconColor group =
      AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
          "test_app1", GetRedIconWithBackgroundColorOf(SK_ColorBLACK));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_BLACK);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a white background color.
  group = AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
      "test_app2", GetRedIconWithBackgroundColorOf(SK_ColorWHITE));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_WHITE);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a red background color.
  group = AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
      "test_app3", GetRedIconWithBackgroundColorOf(SK_ColorRED));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_RED);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a yellow background color.
  group = AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
      "test_app4", GetRedIconWithBackgroundColorOf(SK_ColorYELLOW));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_YELLOW);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a green background color.
  group = AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
      "test_app5", GetRedIconWithBackgroundColorOf(SK_ColorGREEN));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_GREEN);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a blue background color.
  group = AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
      "test_app6", GetRedIconWithBackgroundColorOf(SK_ColorBLUE));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_BLUE);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a magenta background color.
  group = AppIconColorCache::GetInstance(profile()).GetIconColorForApp(
      "test_app7", GetRedIconWithBackgroundColorOf(SK_ColorMAGENTA));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_MAGENTA);
  EXPECT_EQ(group.hue(), red_hue);
}

}  // namespace ash
