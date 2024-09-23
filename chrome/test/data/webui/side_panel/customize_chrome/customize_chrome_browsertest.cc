// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"

class SidePanelCustomizeChromeTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelCustomizeChromeTest() {
    set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kCustomizeChromeWallpaperSearch,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, ButtonLabel) {
  RunTest("side_panel/customize_chrome/button_label_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Cards) {
  RunTest("side_panel/customize_chrome/cards_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Shortcuts) {
  RunTest("side_panel/customize_chrome/shortcuts_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, App) {
  RunTest("side_panel/customize_chrome/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Appearance) {
  RunTest("side_panel/customize_chrome/appearance_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Categories) {
  RunTest("side_panel/customize_chrome/categories_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, CheckMarkWrapper) {
  RunTest("side_panel/customize_chrome/check_mark_wrapper_test.js",
          "mocha.run()");
}
IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Combobox) {
  RunTest("side_panel/customize_chrome/wallpaper_search/combobox_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, HoverButton) {
  RunTest("side_panel/customize_chrome/hover_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Themes) {
  RunTest("side_panel/customize_chrome/themes_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, ThemeSnapshot) {
  RunTest("side_panel/customize_chrome/theme_snapshot_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeTest, Toolbar) {
  RunTest("side_panel/customize_chrome/toolbar_test.js", "mocha.run()");
}

using CustomizeChromeWallpaperSearchTest = SidePanelCustomizeChromeTest;

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Misc) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Misc')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Descriptors) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Descriptors')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Search) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Search')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, History) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest History')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Error) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Error')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Feedback) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Feedback')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Metrics) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Metrics')");
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeWallpaperSearchTest, Inspiration) {
  RunTest(
      "side_panel/customize_chrome/wallpaper_search/wallpaper_search_test.js",
      "runMochaSuite('WallpaperSearchTest Inspiration')");
}
