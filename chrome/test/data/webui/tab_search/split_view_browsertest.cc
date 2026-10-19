// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SplitViewTest : public WebUIMochaBrowserTest {
 protected:
  SplitViewTest() { set_test_loader_host(chrome::kChromeUITabSearchHost); }
};

IN_PROC_BROWSER_TEST_F(SplitViewTest, SplitNewTabPage) {
  RunTest("tab_search/split_new_tab_page_test.js", "mocha.run()");
}
