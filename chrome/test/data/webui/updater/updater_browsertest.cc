// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class UpdaterBrowserTest : public WebUIMochaBrowserTest {
 protected:
  UpdaterBrowserTest() { set_test_loader_host(chrome::kChromeUIUpdaterHost); }
};

typedef UpdaterBrowserTest UpdaterAppTest;
IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EventHistoryTest) {
  RunTest("updater/event_history_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterBarTest) {
  RunTest("updater/event_list/filter_bar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterDialogTest) {
  RunTest("updater/event_list/filter_dialog/filter_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, AppDialogTest) {
  RunTest("updater/event_list/filter_dialog/app_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EventDialogTest) {
  RunTest("updater/event_list/filter_dialog/event_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, OutcomeDialogTest) {
  RunTest("updater/event_list/filter_dialog/outcome_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterDialogFooterTest) {
  RunTest("updater/event_list/filter_dialog/filter_dialog_footer_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, DateDialogTest) {
  RunTest("updater/event_list/filter_dialog/date_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, TypeDialogTest) {
  RunTest("updater/event_list/filter_dialog/type_dialog_test.js",
          "mocha.run();");
}
