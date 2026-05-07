// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

class WebUiToolbarJsTest : public WebUIMochaBrowserTest {
 public:
  WebUiToolbarJsTest() {
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

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsTest, ReadOnlyOmnibox) {
  RunTest("webui_toolbar/readonly_omnibox_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsTest, PinnedToolbarAction) {
  RunTest("webui_toolbar/pinned_toolbar_action_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsTest, LocationBar) {
  RunTest("webui_toolbar/location_bar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsTest, LocationIcon) {
  RunTest("webui_toolbar/location_icon_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiToolbarJsTest, PermissionChip) {
  RunTest("webui_toolbar/permission_chip_test.js", "mocha.run();");
}
