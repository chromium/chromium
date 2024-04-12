// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

using State = LensOverlayController::State;

class LensWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  LensWebUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(chrome::kChromeUILensHost);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    WebUIMochaBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      lens::features::kLensOverlay};
};

class LensOverlayTest : public LensWebUIBrowserTest {
 protected:
  void RunOverlayTest(const std::string& file, const std::string& trigger) {
    WaitForPaint();

    // State should start in off.
    auto* controller = browser()
                           ->tab_strip_model()
                           ->GetActiveTab()
                           ->tab_features()
                           ->lens_overlay_controller();
    ASSERT_EQ(controller->state(), State::kOff);

    // Showing UI should eventually result in overlay state.
    controller->ShowUI();
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return controller->state() == State::kOverlay; }));

    // Get the overlay webview and wait for WebUI to finish loading.
    raw_ptr<views::WebView> overlay_web_view =
        views::AsViewClass<views::WebView>(
            controller->GetOverlayWidgetForTesting()
                ->GetContentsView()
                ->children()[0]);
    auto* web_contents = overlay_web_view->GetWebContents();
    content::WaitForLoadStop(web_contents);
    ASSERT_TRUE(RunTestOnWebContents(web_contents, file, trigger, true));

    // Clean up (the searchbox handler will leave a dangling pointer if not
    // explicitly destroyed).
    controller->ResetSearchboxHandler();
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
          ->contents()
          ->CompletedFirstVisuallyNonEmptyPaint();
    }));
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayCloseButton) {
  RunOverlayTest("lens/overlay/overlay_close_button_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(LensOverlayTest, SelectionOverlay) {
  RunOverlayTest("lens/overlay/selection_overlay_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, PostSelectionRenderer) {
  RunOverlayTest("lens/overlay/post_selection_renderer_test.js", "mocha.run()");
}

using LensSidePanelTest = LensOverlayTest;
IN_PROC_BROWSER_TEST_F(LensSidePanelTest, SidePanelResultsFrame) {
  RunOverlayTest("lens/side_panel/results_frame_test.js", "mocha.run()");
}

}  // namespace
