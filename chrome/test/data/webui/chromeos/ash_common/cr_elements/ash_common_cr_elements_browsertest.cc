// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/compositor_switches.h"

typedef WebUIMochaBrowserTest AshCommonCrElementsTest;

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrA11yAnnouncer) {
  RunTest("chromeos/ash_common/cr_elements/cr_a11y_announcer_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrAutoImg) {
  RunTest("chromeos/ash_common/cr_elements/cr_auto_img_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrLinkRow) {
  RunTest("chromeos/ash_common/cr_elements/cr_link_row_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrLazyRender) {
  RunTest("chromeos/ash_common/cr_elements/cr_lazy_render_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrPolicyIndicator) {
  RunTest("chromeos/ash_common/cr_elements/cr_policy_indicator_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrPolicyPrefIndicator) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUIOSSettingsHost);
  RunTest("chromeos/ash_common/cr_elements/cr_policy_pref_indicator_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrPolicyIndicatorMixin) {
  RunTest("chromeos/ash_common/cr_elements/cr_policy_indicator_mixin_test.js",
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

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, CrScrollableMixin) {
  RunTest("chromeos/ash_common/cr_elements/cr_scrollable_mixin_test.js",
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

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, FindShortcutMixin) {
  RunTest("chromeos/ash_common/cr_elements/find_shortcut_mixin_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, I18nMixin) {
  RunTest("chromeos/ash_common/cr_elements/i18n_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, LocalizedLink) {
  RunTest("chromeos/ash_common/cr_elements/localized_link_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, ListPropertyUpdateMixin) {
  RunTest("chromeos/ash_common/cr_elements/list_property_update_mixin_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, StoreClient) {
  RunTest("chromeos/ash_common/cr_elements/store_client_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AshCommonCrElementsTest, WebUiListenerMixin) {
  RunTest("chromeos/ash_common/cr_elements/web_ui_listener_mixin_test.js",
          "mocha.run()");
}

// Test with --enable-pixel-output-in-tests enabled, required by a few test
// cases using HTML canvas.
class AshCommonCrElementsWithPixelOutputTest : public WebUIMochaBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

// TOD(crbug.com/906991): revisit after PlzDedicatedWorker launch.
IN_PROC_BROWSER_TEST_F(AshCommonCrElementsWithPixelOutputTest,
                       DISABLED_CrLottie) {
  RunTest("chromeos/ash_common/cr_elements/cr_lottie_test.js", "mocha.run()");
}
