// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class TabStripTest : public WebUIMochaBrowserTest {
 protected:
  TabStripTest() { set_test_loader_host(chrome::kChromeUITabStripHost); }
};

IN_PROC_BROWSER_TEST_F(TabStripTest, Tab) {
  RunTest("tab_strip/tab_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabStripTest, AlertIndicators) {
  RunTest("tab_strip/alert_indicators_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabStripTest, AlertIndicator) {
  RunTest("tab_strip/alert_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabStripTest, TabSwiper) {
  RunTest("tab_strip/tab_swiper_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabStripTest, TabGroup) {
  RunTest("tab_strip/tab_group_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabStripTest, DragManager) {
  RunTest("tab_strip/drag_manager_test.js", "mocha.run()");
}
