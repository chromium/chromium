// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs CUJ tests for lens overlay. These tests simulate input events
// and cannot be run in parallel.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

class LensOverlayControllerCUJTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void WaitForTemplateURLServiceToLoad() {
    auto* const template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  InteractiveTestApi::MultiStep OpenLensOverlay() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
    const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

    // In kDocumentWithNamedElement.
    const DeepQuery kPathToBody{
        "body",
    };

    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kFirstPaintState);
    return Steps(
        InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
        EnsurePresent(kActiveTab, kPathToBody),
        // TODO(https://crbug.com/331859922): This functionality should be built
        // into test framework.
        PollState(kFirstPaintState,
                  [this]() {
                    return browser()
                        ->tab_strip_model()
                        ->GetActiveTab()
                        ->contents()
                        ->CompletedFirstVisuallyNonEmptyPaint();
                  }),
        WaitForState(kFirstPaintState, true),
        MoveMouseTo(kActiveTab, kPathToBody), ClickMouse(ui_controls::RIGHT),
        WaitForShow(RenderViewContextMenu::kRegionSearchItem),
        FlushEvents(),  // Required to fully render the menu before selection.

        SelectMenuItem(RenderViewContextMenu::kRegionSearchItem));
  }

 private:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensOverlay};
};

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User clicks the "close" button to close lens overlay.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, OpenAndClose) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  // In kDocumentWithNamedElement.
  const DeepQuery kPathToBody{
      "body",
  };

  // In the lens overlay.
  const DeepQuery kPathToCloseButton{
      "lens-overlay-app",
      "#closeButton",
  };
  constexpr char kClickFn[] = "(el) => { el.click(); }";

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(Steps(InstrumentNonTabWebView(
                             kOverlayId, LensOverlayController::kOverlayId),
                         WaitForWebContentsReady(
                             kOverlayId, GURL("chrome-untrusted://lens")))),
      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(Steps(FlushEvents(),
                          EnsurePresent(kOverlayId, kPathToCloseButton),
                          ExecuteJsAt(kOverlayId, kPathToCloseButton, kClickFn,
                                      ExecuteJsMode::kFireAndForget),
                          WaitForHide(kOverlayId))));
}

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User drags to select a manual region on the overlay.
//  (4) Side panel opens with results.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, SelectManualRegion) {
  WaitForTemplateURLServiceToLoad();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  const DeepQuery kPathToRegionSelection{
      "lens-overlay-app",
      "lens-selection-overlay",
      "#regionSelectionLayer",
  };
  const DeepQuery kPathToResultsFrame{
      "lens-side-panel-app",
      "#results",
  };

  auto off_center_point = base::BindLambdaForTesting([browser_view]() {
    gfx::Point off_center =
        browser_view->contents_web_view()->bounds().CenterPoint();
    off_center.Offset(100, 100);
    return off_center;
  });

  RunTestSequence(
      OpenLensOverlay(),

      // The overlay controller is an independent floating widget
      // associated with a tab rather than a browser window, so by
      // convention gets its own element context.
      InAnyContext(Steps(InstrumentNonTabWebView(
                             kOverlayId, LensOverlayController::kOverlayId),
                         WaitForWebContentsReady(
                             kOverlayId, GURL("chrome-untrusted://lens")))),
      // Wait for the webview to finish loading to prevent re-entrancy. Then do
      // a drag offset from the center. Flush tasks after drag to prevent
      // flakiness.
      InSameContext(Steps(FlushEvents(),
                          WaitForShow(LensOverlayController::kOverlayId),
                          EnsurePresent(kOverlayId, kPathToRegionSelection),
                          MoveMouseTo(LensOverlayController::kOverlayId),
                          DragMouseTo(off_center_point))),

      // The drag should have opened the side panel with the results frame.
      InAnyContext(Steps(
          FlushEvents(),
          InstrumentNonTabWebView(
              kOverlaySidePanelWebViewId,
              LensOverlayController::kOverlaySidePanelWebViewId),
          FlushEvents(),
          EnsurePresent(kOverlaySidePanelWebViewId, kPathToResultsFrame))));
}

}  // namespace
