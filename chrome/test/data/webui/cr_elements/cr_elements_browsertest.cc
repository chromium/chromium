// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/compositor_switches.h"

typedef WebUIMochaBrowserTest CrElementsTest;

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrA11yAnnouncer) {
  RunTest("cr_elements/cr_a11y_announcer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrButton) {
  RunTest("cr_elements/cr_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrContainerShadowMixin) {
  RunTest("cr_elements/cr_container_shadow_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrDialog) {
  RunTest("cr_elements/cr_dialog_test.js", "mocha.run()");
}

// https://crbug.com/1008122 - Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CrDrawer DISABLED_CrDrawer
#else
#define MAYBE_CrDrawer CrDrawer
#endif
IN_PROC_BROWSER_TEST_F(CrElementsTest, MAYBE_CrDrawer) {
  RunTest("cr_elements/cr_drawer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrExpandButton) {
  RunTest("cr_elements/cr_expand_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, FindShortcutMixin) {
  RunTest("cr_elements/find_shortcut_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, I18nMixin) {
  RunTest("cr_elements/i18n_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, IconButton) {
  RunTest("cr_elements/cr_icon_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLazyRender) {
  RunTest("cr_elements/cr_lazy_render_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLinkRow) {
  RunTest("cr_elements/cr_link_row_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, ListPropertyUpdateMixin) {
  RunTest("cr_elements/list_property_update_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrRadioButton) {
  RunTest("cr_elements/cr_radio_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrCardRadioButton) {
  RunTest("cr_elements/cr_card_radio_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrRadioGroup) {
  RunTest("cr_elements/cr_radio_group_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrScrollableMixin) {
  RunTest("cr_elements/cr_scrollable_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrSearchField) {
  RunTest("cr_elements/cr_search_field_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(CrElementsTest, CrSearchableDropDown) {
  RunTest("cr_elements/cr_searchable_drop_down_test.js", "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrSlider) {
  RunTest("cr_elements/cr_slider_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrSplitter) {
  RunTest("cr_elements/cr_splitter_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrToast) {
  RunTest("cr_elements/cr_toast_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrToastManager) {
  RunTest("cr_elements/cr_toast_manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrViewManager) {
  RunTest("cr_elements/cr_view_manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrPolicyIndicator) {
  RunTest("cr_elements/cr_policy_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrPolicyPrefIndicator) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_elements/cr_policy_pref_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrPolicyIndicatorMixin) {
  RunTest("cr_elements/cr_policy_indicator_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrAutoImg) {
  RunTest("cr_elements/cr_auto_img_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrToolbar) {
  RunTest("cr_elements/cr_toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrTree) {
  RunTest("cr_elements/cr_tree_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, WebUiListenerMixin) {
  RunTest("cr_elements/web_ui_listener_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrUrlListItem) {
  RunTest("cr_elements/cr_url_list_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, StoreClient) {
  RunTest("cr_elements/store_client_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLoadingGradient) {
  RunTest("cr_elements/cr_loading_gradient_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrFeedbackButtons) {
  RunTest("cr_elements/cr_feedback_buttons_test.js", "mocha.run()");
}

// Test with --enable-pixel-output-in-tests enabled, required by a few test
// cases using HTML canvas.
class CrElementsWithPixelOutputTest : public WebUIMochaBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

// TOD(crbug.com/906991): revisit after PlzDedicatedWorker launch.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_CrLottie DISABLED_CrLottie
#else
#define MAYBE_CrLottie CrLottie
#endif
IN_PROC_BROWSER_TEST_F(CrElementsWithPixelOutputTest, MAYBE_CrLottie) {
  RunTest("cr_elements/cr_lottie_test.js", "mocha.run()");
}

// https://crbug.com/1044390 - maybe flaky on Mac?
#if BUILDFLAG(IS_MAC)
#define MAYBE_CrFingerprintProgressArc DISABLED_CrFingerprintProgressArc
#else
#define MAYBE_CrFingerprintProgressArc CrFingerprintProgressArc
#endif
IN_PROC_BROWSER_TEST_F(CrElementsWithPixelOutputTest,
                       MAYBE_CrFingerprintProgressArc) {
  RunTest("cr_elements/cr_fingerprint_progress_arc_test.js", "mocha.run()");
}
