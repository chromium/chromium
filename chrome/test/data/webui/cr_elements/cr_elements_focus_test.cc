// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrElementsFocusTest;

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrActionMenu) {
  RunTest("cr_elements/cr_action_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrCheckbox) {
  RunTest("cr_elements/cr_checkbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, InfiniteList) {
  RunTest("cr_elements/cr_infinite_list_test.js",
          "runMochaSuite('InfiniteListFocusTest')");
}

// https://crbug.com/997943: Flaky on Mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_CrInput DISABLED_CrInput
#else
#define MAYBE_CrInput CrInput
#endif
IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, MAYBE_CrInput) {
  RunTest("cr_elements/cr_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrProfileAvatarSelector) {
  RunTest("cr_elements/cr_profile_avatar_selector_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrTabs) {
  RunTest("cr_elements/cr_tabs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrToggle) {
  RunTest("cr_elements/cr_toggle_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrToolbarSearchField) {
  RunTest("cr_elements/cr_toolbar_search_field_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, IronList) {
  RunTest("cr_elements/iron_list_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrGrid) {
  RunTest("cr_elements/cr_grid_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrMenuSelector) {
  RunTest("cr_elements/cr_menu_selector_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrToolbar) {
  RunTest("cr_elements/cr_toolbar_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrTextarea) {
  RunTest("cr_elements/cr_textarea_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, CrFocusRowMixin) {
  RunTest("cr_elements/cr_focus_row_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsFocusTest, FocusRowMixinLit) {
  RunTest("cr_elements/focus_row_mixin_lit_test.js", "mocha.run()");
}
