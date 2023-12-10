// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"

using SidePanelReadingListTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelReadingListTest, App) {
  set_test_loader_host(chrome::kChromeUIReadLaterHost);
  RunTest("side_panel/reading_list/reading_list_app_test.js", "mocha.run()");
}

class ReadAnythingMochaBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ReadAnythingMochaBrowserTest() {
    set_test_loader_host(chrome::kChromeUIUntrustedReadAnythingSidePanelHost);
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
  }

  base::test::ScopedFeatureList scoped_feature_list_{features::kReadAnything};
};

using ReadAnythingMochaTest = ReadAnythingMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, CheckmarkVisibleOnSelected) {
  RunTest("side_panel/read_anything/checkmark_visible_on_selected.js",
          "mocha.run()");
}
