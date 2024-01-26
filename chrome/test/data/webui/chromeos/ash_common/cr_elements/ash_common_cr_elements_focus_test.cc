// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest AshCommonCrElementsFocusTest;

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrActionMenu) {
  RunTest("chromeos/ash_common/cr_elements/cr_action_menu_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrCheckbox) {
  RunTest("chromeos/ash_common/cr_elements/cr_checkbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrFocusRowMixin) {
  RunTest("chromeos/ash_common/cr_elements/cr_focus_row_mixin_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrInput) {
  RunTest("chromeos/ash_common/cr_elements/cr_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrMenuSelector) {
  RunTest("chromeos/ash_common/cr_elements/cr_menu_selector_focus_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrTextarea) {
  RunTest("chromeos/ash_common/cr_elements/cr_textarea_focus_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrTabs) {
  RunTest("chromeos/ash_common/cr_elements/cr_tabs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrToggle) {
  RunTest("chromeos/ash_common/cr_elements/cr_toggle_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrToolbar) {
  RunTest("chromeos/ash_common/cr_elements/cr_toolbar_focus_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsFocusTest, CrToolbarSearchField) {
  RunTest("chromeos/ash_common/cr_elements/cr_toolbar_search_field_test.js",
          "mocha.run()");
}
