// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapp_icon.h"

#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapps {

namespace {

const int kIdealHomescreenIconSize = 96;
const int kMinimumHomescreenIconSize = 48;
const int kIdealSplashImageSize = 256;
const int kMinimumSplashImageSize = 32;
const int kIdealMonochromeIconSize = 48;
const int kIdealAdaptiveLauncherIconSize = 166;
const int kIdealShortcutIconSize = 72;

}  // namespace

class WebappIconTest : public testing::Test {
 public:
  WebappIconTest() = default;

  WebappIconTest(const WebappIconTest&) = delete;
  WebappIconTest& operator=(const WebappIconTest&) = delete;

 protected:
  void SetUp() override {
    WebappsIconUtils::SetIconSizesForTesting({
        kIdealHomescreenIconSize,
        kMinimumHomescreenIconSize,
        kIdealSplashImageSize,
        kMinimumSplashImageSize,
        kIdealMonochromeIconSize,
        kIdealAdaptiveLauncherIconSize,
        kIdealShortcutIconSize,
    });
  }

  const GURL kIconUrl = GURL("http://example.com");
};

TEST_F(WebappIconTest, GetIdealIconSizes) {
  WebappIcon primary_icon(kIconUrl, false /* is_maskable*/,
                          webapk::Image::PRIMARY_ICON);
  EXPECT_EQ(primary_icon.GetIdealSizeInPx(), kIdealHomescreenIconSize);

  WebappIcon maskable_primary_icon(kIconUrl, true /* is_maskable*/,
                                   webapk::Image::PRIMARY_ICON);
  EXPECT_EQ(maskable_primary_icon.GetIdealSizeInPx(),
            kIdealAdaptiveLauncherIconSize);

  WebappIcon splash_icon(kIconUrl, false /* is_maskable*/,
                         webapk::Image::SPLASH_ICON);
  EXPECT_EQ(splash_icon.GetIdealSizeInPx(), kIdealSplashImageSize);

  WebappIcon shortcut_icon(kIconUrl, false /* is_maskable*/,
                           webapk::Image::SHORTCUT_ICON);
  EXPECT_EQ(shortcut_icon.GetIdealSizeInPx(), kIdealShortcutIconSize);
}

// Test that icon with multiple usages have ideal size of the largest usage.
TEST_F(WebappIconTest, GetIdealSizeForMultiplePurpose) {
  WebappIcon icon(kIconUrl);
  icon.AddUsage(webapk::Image::PRIMARY_ICON);
  icon.AddUsage(webapk::Image::SPLASH_ICON);
  EXPECT_EQ(icon.GetIdealSizeInPx(), kIdealSplashImageSize);

  WebappIcon icon2(kIconUrl);
  icon2.AddUsage(webapk::Image::PRIMARY_ICON);
  icon2.AddUsage(webapk::Image::SHORTCUT_ICON);
  EXPECT_EQ(icon2.GetIdealSizeInPx(), kIdealHomescreenIconSize);

  WebappIcon icon3(kIconUrl);
  icon3.AddUsage(webapk::Image::PRIMARY_ICON);
  icon3.AddUsage(webapk::Image::SPLASH_ICON);
  icon3.AddUsage(webapk::Image::SHORTCUT_ICON);
  EXPECT_EQ(icon3.GetIdealSizeInPx(), kIdealSplashImageSize);
}

}  // namespace webapps
