// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"
#include "ui/compositor/compositor_switches.h"

typedef WebUIMochaBrowserTest CrElementsTest;

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrA11yAnnouncer) {
  RunTest("cr_elements/cr_a11y_announcer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrButton) {
  RunTest("cr_elements/cr_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrChip) {
  RunTest("cr_elements/cr_chip_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrCollapse) {
  RunTest("cr_elements/cr_collapse_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrContainerShadowMixin) {
  RunTest("cr_elements/cr_container_shadow_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrContainerShadowMixinLit) {
  RunTest("cr_elements/cr_container_shadow_mixin_lit_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrIcon) {
  RunTest("cr_elements/cr_icon_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrIconset) {
  RunTest("cr_elements/cr_iconset_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, FindShortcutMixin) {
  RunTest("cr_elements/find_shortcut_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, FindShortcutMixinLit) {
  RunTest("cr_elements/find_shortcut_mixin_lit_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, I18nMixin) {
  RunTest("cr_elements/i18n_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, I18nMixinLit) {
  RunTest("cr_elements/i18n_mixin_lit_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrIconButton) {
  RunTest("cr_elements/cr_icon_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrInfiniteList) {
  RunTest("cr_elements/cr_infinite_list_test.js",
          "runMochaSuite('InfiniteListTest')");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLazyList) {
  RunTest("cr_elements/cr_lazy_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLazyRender) {
  RunTest("cr_elements/cr_lazy_render_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLazyRenderLit) {
  RunTest("cr_elements/cr_lazy_render_lit_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrRipple) {
  RunTest("cr_elements/cr_ripple_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrRippleMixin) {
  RunTest("cr_elements/cr_ripple_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrCardRadioButton) {
  RunTest("cr_elements/cr_card_radio_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrRadioGroup) {
  RunTest("cr_elements/cr_radio_group_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrScrollObserverMixin) {
  RunTest("cr_elements/cr_scroll_observer_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrScrollObserverMixinLit) {
  RunTest("cr_elements/cr_scroll_observer_mixin_lit_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrSearchField) {
  RunTest("cr_elements/cr_search_field_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrSelectableMixin) {
  RunTest("cr_elements/cr_selectable_mixin_test.js", "mocha.run()");
}

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

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrPageSelector) {
  RunTest("cr_elements/cr_page_selector_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrPolicyIndicator) {
  RunTest("cr_elements/cr_policy_indicator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrProgress) {
  RunTest("cr_elements/cr_progress_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrAutoImg) {
  RunTest("cr_elements/cr_auto_img_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrToolbar) {
  RunTest("cr_elements/cr_toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrTooltip) {
  RunTest("cr_elements/cr_tooltip_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrTree) {
  RunTest("cr_elements/cr_tree_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, WebUiListenerMixin) {
  RunTest("cr_elements/web_ui_listener_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, WebUiListenerMixinLit) {
  RunTest("cr_elements/web_ui_listener_mixin_lit_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrUrlListItem) {
  RunTest("cr_elements/cr_url_list_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, StoreClient) {
  RunTest("cr_elements/store_client_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrElementsTest, CrLitElement) {
  RunTest("cr_elements/cr_lit_element_test.js", "mocha.run()");
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
  CrElementsWithPixelOutputTest() {
    // Disable PlzDedicatedWorker flag for this test since it causes the
    // test to hang. Revisit after launch, see crbug.com/906991.
    scoped_feature_list_.InitWithFeatures(
        {}, {blink::features::kPlzDedicatedWorker});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrElementsWithPixelOutputTest, CrLottie) {
  RunTest("cr_elements/cr_lottie_test.js", "mocha.run()");
}
