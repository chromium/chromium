// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest AshCommonCrElementsTest;

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrA11yAnnouncer) {
  RunTest("chromeos/ash_common/cr_elements/cr_a11y_announcer_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrButton) {
  RunTest("chromeos/ash_common/cr_elements/cr_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrContainerShadowMixin) {
  RunTest("chromeos/ash_common/cr_elements/cr_container_shadow_mixin_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrDialog) {
  RunTest("chromeos/ash_common/cr_elements/cr_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrDrawer) {
  RunTest("chromeos/ash_common/cr_elements/cr_drawer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrExpandButton) {
  RunTest("chromeos/ash_common/cr_elements/cr_expand_button_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrIconButton) {
  RunTest("chromeos/ash_common/cr_elements/cr_icon_button_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrLazyRender) {
  RunTest("chromeos/ash_common/cr_elements/cr_lazy_render_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrRadioButton) {
  RunTest("chromeos/ash_common/cr_elements/cr_radio_button_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrCardRadioButton) {
  RunTest("chromeos/ash_common/cr_elements/cr_card_radio_button_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrRadioGroup) {
  RunTest("chromeos/ash_common/cr_elements/cr_radio_group_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrSearchField) {
  RunTest("chromeos/ash_common/cr_elements/cr_search_field_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrSearchableDropDown) {
  RunTest("chromeos/ash_common/cr_elements/cr_searchable_drop_down_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrSlider) {
  RunTest("chromeos/ash_common/cr_elements/cr_slider_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrToast) {
  RunTest("chromeos/ash_common/cr_elements/cr_toast_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrToastManager) {
  RunTest("chromeos/ash_common/cr_elements/cr_toast_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrToolbar) {
  RunTest("chromeos/ash_common/cr_elements/cr_toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrViewManager) {
  RunTest("chromeos/ash_common/cr_elements/cr_view_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, I18nMixin) {
  RunTest("chromeos/ash_common/cr_elements/i18n_mixin_test.js", "mocha.run()");
}
