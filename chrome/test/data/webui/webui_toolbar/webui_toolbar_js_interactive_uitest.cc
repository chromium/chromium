// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/native_theme/mock_os_settings_provider.h"

class WebUiToolbarJsInteractiveUiTest : public WebUIMochaFocusTest {
 public:
  WebUiToolbarJsInteractiveUiTest() {
    set_test_loader_host(chrome::kChromeUIWebUIToolbarHost);

    // Need features::IsWebUIToolbarEnabled() to return true
    // to init the WebUI URL we use.
    scoped_feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUILocationBar}, {});
    CHECK(features::IsWebUIToolbarEnabled());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsInteractiveUiTest, ReadOnlyOmnibox) {
  RunTest("webui_toolbar/readonly_omnibox_focus_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsInteractiveUiTest, LocationBar) {
  RunTest("webui_toolbar/location_bar_focus_test.js", "mocha.run();");
}

class WebUiToolbarHighContrastJsInteractiveUiTest
    : public WebUiToolbarJsInteractiveUiTest {
 public:
  void SetUpOnMainThread() override {
    WebUiToolbarJsInteractiveUiTest::SetUpOnMainThread();
    os_settings_provider_.SetForcedColorsActive(true);
    os_settings_provider_.SetPreferredContrast(
        ui::NativeTheme::PreferredContrast::kMore);
  }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
};

IN_PROC_BROWSER_TEST_F(WebUiToolbarHighContrastJsInteractiveUiTest,
                       LocationBarHighContrast) {
  RunTest("webui_toolbar/location_bar_high_contrast_focus_test.js",
          "mocha.run();");
}
