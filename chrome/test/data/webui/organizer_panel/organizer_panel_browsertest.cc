// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using OrganizerPanelTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(OrganizerPanelTest, App) {
  set_test_loader_host(chrome::kChromeUIOrganizerPanelHost);
  RunTest("organizer_panel/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelTest, ListSectionItem) {
  set_test_loader_host(chrome::kChromeUIOrganizerPanelHost);
  RunTest("organizer_panel/organizer_list_section_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelTest, ListSection) {
  set_test_loader_host(chrome::kChromeUIOrganizerPanelHost);
  RunTest("organizer_panel/organizer_list_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelTest, List) {
  set_test_loader_host(chrome::kChromeUIOrganizerPanelHost);
  RunTest("organizer_panel/organizer_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelTest, OpenTabsDelegate) {
  set_test_loader_host(chrome::kChromeUIOrganizerPanelHost);
  RunTest("organizer_panel/open_tabs_delegate_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelTest, RecentTabsDelegate) {
  set_test_loader_host(chrome::kChromeUIOrganizerPanelHost);
  RunTest("organizer_panel/recent_tabs_delegate_test.js", "mocha.run()");
}
