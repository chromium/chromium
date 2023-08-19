// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class HistoryFocusTest : public WebUIMochaFocusTest {
 protected:
  HistoryFocusTest() { set_test_loader_host(chrome::kChromeUIHistoryHost); }
};

// Flaky, https://crbug.com/1200678
#if BUILDFLAG(IS_MAC)
#define MAYBE_Toolbar DISABLED_Toolbar
#else
#define MAYBE_Toolbar Toolbar
#endif
IN_PROC_BROWSER_TEST_F(HistoryFocusTest, MAYBE_Toolbar) {
  RunTest("history/history_toolbar_focus_test.js", "mocha.run()");
}
#undef MAYBE_All

// Flaky. See crbug.com/1040940.
IN_PROC_BROWSER_TEST_F(HistoryFocusTest, DISABLED_List) {
  RunTest("history/history_list_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryFocusTest, SyncedDeviceManager) {
  RunTest("history/history_synced_device_manager_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(HistoryFocusTest, Item) {
  RunTest("history/history_item_focus_test.js", "mocha.run()");
}
