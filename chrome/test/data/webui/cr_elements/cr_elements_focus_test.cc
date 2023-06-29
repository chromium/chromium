// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrElementsActionMenuTest;
IN_PROC_BROWSER_TEST_F(CrElementsActionMenuTest, All) {
  RunTest("cr_elements/cr_action_menu_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsCheckboxTest;
IN_PROC_BROWSER_TEST_F(CrElementsCheckboxTest, All) {
  RunTest("cr_elements/cr_checkbox_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsInputTest;

// https://crbug.com/997943: Flaky on Mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif
IN_PROC_BROWSER_TEST_F(CrElementsInputTest, MAYBE_All) {
  RunTest("cr_elements/cr_input_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsProfileAvatarSelectorTest;
IN_PROC_BROWSER_TEST_F(CrElementsProfileAvatarSelectorTest, All) {
  RunTest("cr_elements/cr_profile_avatar_selector_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsTabsTest;
IN_PROC_BROWSER_TEST_F(CrElementsTabsTest, All) {
  RunTest("cr_elements/cr_tabs_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsToggleTest;
IN_PROC_BROWSER_TEST_F(CrElementsToggleTest, All) {
  RunTest("cr_elements/cr_toggle_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsToolbarSearchFieldTest;
IN_PROC_BROWSER_TEST_F(CrElementsToolbarSearchFieldTest, All) {
  RunTest("cr_elements/cr_toolbar_search_field_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest IronListFocusTest;
IN_PROC_BROWSER_TEST_F(IronListFocusTest, All) {
  RunTest("cr_elements/iron_list_focus_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsGridFocusTest;
IN_PROC_BROWSER_TEST_F(CrElementsGridFocusTest, All) {
  RunTest("cr_elements/cr_grid_focus_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsMenuSelectorFocusTest;
IN_PROC_BROWSER_TEST_F(CrElementsMenuSelectorFocusTest, All) {
  RunTest("cr_elements/cr_menu_selector_focus_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsToolbarFocusTest;
IN_PROC_BROWSER_TEST_F(CrElementsToolbarFocusTest, All) {
  RunTest("cr_elements/cr_toolbar_focus_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrElementsTextareaFocusTest;
IN_PROC_BROWSER_TEST_F(CrElementsTextareaFocusTest, All) {
  RunTest("cr_elements/cr_textarea_focus_test.js", "mocha.run()");
}

typedef WebUIMochaFocusTest CrFocusRowMixinTest;
IN_PROC_BROWSER_TEST_F(CrFocusRowMixinTest, FocusTest) {
  RunTest("cr_elements/cr_focus_row_mixin_test.js", "mocha.run()");
}
