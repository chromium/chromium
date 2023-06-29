// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/compositor_switches.h"

// Test with --enable-pixel-output-in-tests enabled, required by a few test
// cases using HTML canvas.
class WithPixelOutputTest : public WebUIMochaBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

typedef WebUIMochaBrowserTest CrElementsA11yAnnouncerTest;
IN_PROC_BROWSER_TEST_F(CrElementsA11yAnnouncerTest, All) {
  RunTest("cr_elements/cr_a11y_announcer_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsButtonTest;
IN_PROC_BROWSER_TEST_F(CrElementsButtonTest, All) {
  RunTest("cr_elements/cr_button_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsContainerShadowMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsContainerShadowMixinTest, All) {
  RunTest("cr_elements/cr_container_shadow_mixin_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsDialogTest;
IN_PROC_BROWSER_TEST_F(CrElementsDialogTest, All) {
  RunTest("cr_elements/cr_dialog_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsDrawerTest;

// https://crbug.com/1008122 - Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Drawer DISABLED_Drawer
#else
#define MAYBE_Drawer Drawer
#endif
IN_PROC_BROWSER_TEST_F(CrElementsDrawerTest, MAYBE_Drawer) {
  RunTest("cr_elements/cr_drawer_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsExpandButtonTest;
IN_PROC_BROWSER_TEST_F(CrElementsExpandButtonTest, All) {
  RunTest("cr_elements/cr_expand_button_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsFindShortcutMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsFindShortcutMixinTest, All) {
  RunTest("cr_elements/find_shortcut_mixin_test.js", "mocha.run()");
}

typedef WithPixelOutputTest CrElementsFingerprintProgressArcTest;

// https://crbug.com/1044390 - maybe flaky on Mac?
#if BUILDFLAG(IS_MAC)
#define MAYBE_Fingerprint DISABLED_Fingerprint
#else
#define MAYBE_Fingerprint Fingerprint
#endif
IN_PROC_BROWSER_TEST_F(CrElementsFingerprintProgressArcTest,
                       MAYBE_Fingerprint) {
  RunTest("cr_elements/cr_fingerprint_progress_arc_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsI18nMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsI18nMixinTest, All) {
  RunTest("cr_elements/i18n_mixin_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsIconButtonTest;
IN_PROC_BROWSER_TEST_F(CrElementsIconButtonTest, All) {
  RunTest("cr_elements/cr_icon_button_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsLazyRenderTest;
IN_PROC_BROWSER_TEST_F(CrElementsLazyRenderTest, All) {
  RunTest("cr_elements/cr_lazy_render_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsLinkRowTest;
IN_PROC_BROWSER_TEST_F(CrElementsLinkRowTest, All) {
  RunTest("cr_elements/cr_link_row_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsListPropertyUpdateMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsListPropertyUpdateMixinTest, All) {
  RunTest("cr_elements/list_property_update_mixin_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsRadioButtonTest;
IN_PROC_BROWSER_TEST_F(CrElementsRadioButtonTest, All) {
  RunTest("cr_elements/cr_radio_button_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsCardRadioButtonTest;
IN_PROC_BROWSER_TEST_F(CrElementsCardRadioButtonTest, All) {
  RunTest("cr_elements/cr_card_radio_button_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsRadioGroupTest;
IN_PROC_BROWSER_TEST_F(CrElementsRadioGroupTest, All) {
  RunTest("cr_elements/cr_radio_group_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsScrollableMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsScrollableMixinTest, All) {
  RunTest("cr_elements/cr_scrollable_mixin_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsSearchFieldTest;
IN_PROC_BROWSER_TEST_F(CrElementsSearchFieldTest, All) {
  RunTest("cr_elements/cr_search_field_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
typedef WebUIMochaBrowserTest CrElementsSearchableDropDownTest;
IN_PROC_BROWSER_TEST_F(CrElementsSearchableDropDownTest, All) {
  RunTest("cr_elements/cr_searchable_drop_down_test.js", "mocha.run()");
}
#endif

typedef WebUIMochaBrowserTest CrElementsSliderTest;
IN_PROC_BROWSER_TEST_F(CrElementsSliderTest, All) {
  RunTest("cr_elements/cr_slider_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsSplitterTest;
IN_PROC_BROWSER_TEST_F(CrElementsSplitterTest, All) {
  RunTest("cr_elements/cr_splitter_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsToastTest;
IN_PROC_BROWSER_TEST_F(CrElementsToastTest, All) {
  RunTest("cr_elements/cr_toast_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsToastManagerTest;
IN_PROC_BROWSER_TEST_F(CrElementsToastManagerTest, All) {
  RunTest("cr_elements/cr_toast_manager_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsViewManagerTest;
IN_PROC_BROWSER_TEST_F(CrElementsViewManagerTest, All) {
  RunTest("cr_elements/cr_view_manager_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsPolicyIndicatorTest;
IN_PROC_BROWSER_TEST_F(CrElementsPolicyIndicatorTest, All) {
  RunTest("cr_elements/cr_policy_indicator_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsPolicyPrefIndicatorTest;
IN_PROC_BROWSER_TEST_F(CrElementsPolicyPrefIndicatorTest, All) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_elements/cr_policy_pref_indicator_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsPolicyIndicatorMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsPolicyIndicatorMixinTest, All) {
  RunTest("cr_elements/cr_policy_indicator_mixin_test.js", "mocha.run()");
}

typedef WithPixelOutputTest CrElementsLottieTest;
IN_PROC_BROWSER_TEST_F(CrElementsLottieTest, All) {
  RunTest("cr_elements/cr_lottie_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsAutoImgTest;
IN_PROC_BROWSER_TEST_F(CrElementsAutoImgTest, All) {
  RunTest("cr_elements/cr_auto_img_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsToolbarTest;
IN_PROC_BROWSER_TEST_F(CrElementsToolbarTest, All) {
  RunTest("cr_elements/cr_toolbar_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsTreeTest;
IN_PROC_BROWSER_TEST_F(CrElementsTreeTest, All) {
  RunTest("cr_elements/cr_tree_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsWebUiListenerMixinTest;
IN_PROC_BROWSER_TEST_F(CrElementsWebUiListenerMixinTest, All) {
  RunTest("cr_elements/web_ui_listener_mixin_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrElementsUrlListItemTest;
IN_PROC_BROWSER_TEST_F(CrElementsUrlListItemTest, All) {
  RunTest("cr_elements/cr_url_list_item_test.js", "mocha.run()");
}
