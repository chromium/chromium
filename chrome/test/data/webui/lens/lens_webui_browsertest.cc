// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

using State = LensOverlayController::State;

class LensWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  LensWebUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(chrome::kChromeUILensHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      lens::features::kLensOverlay};
};

class LensOverlayTest : public LensWebUIBrowserTest {
 protected:
  void RunOverlayTest(const std::string& file, const std::string& trigger) {
    // State should start in off.
    auto* controller =
        browser()->tab_strip_model()->GetActiveTab()->lens_overlay_controller();
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
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayTest, OverlayCloseButton) {
  RunOverlayTest("lens/overlay/overlay_close_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, ManualRegionSelection) {
  RunOverlayTest("lens/overlay/region_selection_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensOverlayTest, ManualRegionSelectionCanvas) {
  RunOverlayTest("lens/overlay/region_selection_canvas_test.js", "mocha.run()");
}

using LensSidePanelTest = LensWebUIBrowserTest;
IN_PROC_BROWSER_TEST_F(LensSidePanelTest, SidePanelResultsFrame) {
  RunTest("lens/side_panel/results_frame_test.js", "mocha.run()");
}
