// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class TabSearchTest : public WebUIMochaBrowserTest {
 protected:
  TabSearchTest() { set_test_loader_host(chrome::kChromeUITabSearchHost); }
};

IN_PROC_BROWSER_TEST_F(TabSearchTest, App) {
  RunTest("tab_search/tab_search_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, Page) {
  RunTest("tab_search/tab_search_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, Search) {
  RunTest("tab_search/search_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, SelectableLazyList) {
  RunTest("tab_search/selectable_lazy_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, Item) {
  RunTest("tab_search/tab_search_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, MediaTabs) {
  RunTest("tab_search/tab_search_media_tabs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, Declutter) {
  RunTest("tab_search/declutter_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(TabSearchTest, OrganizationSelector) {
  RunTest("tab_search/tab_organization_selector_test.js", "mocha.run()");
}
