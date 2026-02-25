// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class UpdaterBrowserTest : public WebUIMochaBrowserTest {
 protected:
  UpdaterBrowserTest() { set_test_loader_host(chrome::kChromeUIUpdaterHost); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kUpdaterUI};
};

typedef UpdaterBrowserTest UpdaterAppTest;

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, AppTest) {
  RunTest("updater/app_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, AppListTest) {
  RunTest("updater/app_list/app_list_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, AppDialogTest) {
  RunTest("updater/event_list/filter_dialog/app_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, DateDialogTest) {
  RunTest("updater/event_list/filter_dialog/date_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EventDialogTest) {
  RunTest("updater/event_list/filter_dialog/event_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EventHistoryTest) {
  RunTest("updater/event_history_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EventListItemTest) {
  RunTest("updater/event_list/event_list_item_test.js", "mocha.run();");
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_EventListTest DISABLED_EventListTest
#else
#define MAYBE_EventListTest EventListTest
#endif
IN_PROC_BROWSER_TEST_F(UpdaterAppTest, MAYBE_EventListTest) {
  RunTest("updater/event_list/event_list_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterBarTest) {
  RunTest("updater/event_list/filter_bar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterSettingsTest) {
  RunTest("updater/event_list/filter_settings_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, ToolsTest) {
  RunTest("updater/tools_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EnterprisePolicyValueTest) {
  RunTest("updater/enterprise_policy_table/enterprise_policy_value_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EnterprisePolicyTableSectionTest) {
  RunTest(
      "updater/enterprise_policy_table/enterprise_policy_table_section_test.js",
      "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EnterprisePolicyTableTest) {
  RunTest("updater/enterprise_policy_table/enterprise_policy_table_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterDialogFooterTest) {
  RunTest("updater/event_list/filter_dialog/filter_dialog_footer_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, FilterDialogTest) {
  RunTest("updater/event_list/filter_dialog/filter_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, OutcomeDialogTest) {
  RunTest("updater/event_list/filter_dialog/outcome_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, ScopeDialogTest) {
  RunTest("updater/event_list/filter_dialog/scope_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, TypeDialogTest) {
  RunTest("updater/event_list/filter_dialog/type_dialog_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, EnterpriseCompanionStateCardTest) {
  RunTest("updater/updater_state/enterprise_companion_state_card_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, UpdaterStateCardTest) {
  RunTest("updater/updater_state/updater_state_card_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(UpdaterAppTest, UpdaterStateTest) {
  RunTest("updater/updater_state/updater_state_test.js", "mocha.run();");
}
