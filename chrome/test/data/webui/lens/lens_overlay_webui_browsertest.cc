// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

using State = LensOverlayController::State;

class LensOverlayWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  LensOverlayWebUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(chrome::kChromeUILensOverlayHost);
    scoped_feature_list_.InitWithFeatures(
        {lens::features::kLensOverlay},
        {lens::features::kLensOverlayContextualSearchbox});
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    WebUIMochaBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();

    // Permits sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    WebUIMochaBrowserTest::TearDownOnMainThread();

    // Disallow sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LensOverlayTest : public LensOverlayWebUIBrowserTest {
 protected:
  void RunOverlayTest(const std::string& file, const std::string& trigger) {
    WaitForPaint();

    // State should start in off.
    auto* search_controller = browser()
                                  ->tab_strip_model()
                                  ->GetActiveTab()
                                  ->GetTabFeatures()
                                  ->lens_search_controller();
    auto* overlay_controller = search_controller->lens_overlay_controller();
    ASSERT_EQ(overlay_controller->state(), State::kOff);

    // Showing UI should eventually result in overlay state.
    search_controller->OpenLensOverlay(
        lens::LensOverlayInvocationSource::kAppMenu);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return overlay_controller->state() == State::kOverlay; }));

    // Get the overlay webview and wait for WebUI to finish loading.
    auto* web_contents =
        overlay_controller->GetOverlayWebViewForTesting()->GetWebContents();
    content::WaitForLoadStop(web_contents);
    ASSERT_TRUE(RunTestOnWebContents(web_contents, file, trigger, true));

    // Clean up (the searchbox handler will leave a dangling pointer if not
    // explicitly destroyed).
    search_controller->lens_searchbox_controller()
        ->ResetSidePanelSearchboxHandler();
  }

  // Lens overlay takes a screenshot of the tab. In order to take a screenshot
  // the tab must not be about:blank and must be painted.
  void WaitForPaint() {
    const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return browser()
          ->tab_strip_model()
          ->GetActiveTab()
          ->GetContents()
          ->CompletedFirstVisuallyNonEmptyPaint();
    }));
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayBackgroundScrim) {
  RunOverlayTest("lens/overlay/overlay_background_scrim_test.js",
                 "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayCloseButton) {
  RunOverlayTest("lens/overlay/overlay_close_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayPerformanceTracker) {
  RunOverlayTest("lens/overlay/performance_tracker_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_OverlayCursor DISABLED_OverlayCursor
#else
#define MAYBE_OverlayCursor OverlayCursor
#endif
// TODO(b/357503842): Test is failing on Linux bot.
IN_PROC_BROWSER_TEST_F(LensOverlayTest, MAYBE_OverlayCursor) {
  RunOverlayTest("lens/overlay/overlay_cursor_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayMoreOptionsButton) {
  RunOverlayTest("lens/overlay/overlay_more_options_button_test.js",
                 "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayScreenshot) {
  RunOverlayTest("lens/overlay/overlay_screenshot_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayTheme) {
  RunOverlayTest("lens/overlay/overlay_theme_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, ManualRegionSelection) {
  RunOverlayTest("lens/overlay/region_selection_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, TextSelection) {
  RunOverlayTest("lens/overlay/text_selection_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, ObjectSelection) {
  RunOverlayTest("lens/overlay/object_selection_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest,
                       SelectionOverlayWithoutWordsOrObjects) {
  RunOverlayTest(
      "lens/overlay/selection_overlay_test.js",
      "runMochaSuite('SelectionOverlay WithoutWordsOrObjects')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest,
                       SelectionOverlayWithWords) {
  RunOverlayTest(
      "lens/overlay/selection_overlay_test.js",
      "runMochaSuite('SelectionOverlay WithWords')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest,
                       SelectionOverlayWithObjects) {
  RunOverlayTest(
      "lens/overlay/selection_overlay_test.js",
      "runMochaSuite('SelectionOverlay WithObjects')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest,
                       SelectionOverlayWithTranslatedWords) {
  RunOverlayTest(
      "lens/overlay/selection_overlay_test.js",
      "runMochaSuite('SelectionOverlay WithTranslatedWords')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest,
                       SelectionOverlayWithObjectsAndWords) {
  RunOverlayTest(
      "lens/overlay/selection_overlay_test.js",
      "runMochaSuite('SelectionOverlay WithObjectsAndWords')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest,
                       SelectionOverlayInvocationSourceContextMenuImage) {
  RunOverlayTest(
      "lens/overlay/selection_overlay_test.js",
      "runMochaSuite('SelectionOverlay InvocationSourceContextMenuImage')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, SelectionOverlaySimplifiedSelection) {
  RunOverlayTest("lens/overlay/selection_overlay_test.js",
                 "runMochaSuite('SelectionOverlay SimplifiedSelection')");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, SimplifiedSelection) {
  RunOverlayTest("lens/overlay/simplified_selection_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, PostSelectionRenderer) {
  RunOverlayTest("lens/overlay/post_selection_renderer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, FindWordsInRegion) {
  RunOverlayTest("lens/overlay/find_words_in_region_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, CubicBezier) {
  RunOverlayTest("lens/overlay/cubic_bezier_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, TranslatePromo) {
  RunOverlayTest("lens/overlay/overlay_show_translate_promo_test.js",
                 "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, Searchbox) {
  RunOverlayTest("lens/overlay/searchbox_test.js", "mocha.run()");
}

#if defined(UNDEFINED_SANITIZER)
#define MAYBE_TranslateButton DISABLED_TranslateButton
#else
#define MAYBE_TranslateButton TranslateButton
#endif
// TODO(crbug.com/370882134): flaky on ubsan.
IN_PROC_BROWSER_TEST_F(LensOverlayTest, MAYBE_TranslateButton) {
  RunOverlayTest("lens/overlay/translate_button_test.js", "mocha.run()");
}

}  // namespace
