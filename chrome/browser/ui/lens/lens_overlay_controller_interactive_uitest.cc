// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs CUJ tests for lens overlay. These tests simulate input events
// and cannot be run in parallel.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

class LensOverlayControllerCUJTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensOverlay};
};

// This tests the following CUJ:
//  (1) User navigates to a website.
//  (2) User opens lens overlay.
//  (3) User clicks the "close" button to close lens overlay.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerCUJTest, OpenAndClose) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);

  const DeepQuery kPathToCloseButton{
      "lens-overlay-app",
      "#close-button",
  };
  constexpr char kClickFn[] = "(el) => { el.click(); }";

  RunTestSequence(
      InstrumentTab(kActiveTab), NavigateWebContents(kActiveTab, url),
      // TODO(https://crbug.com/328501283): Use a UI entry point.
      Do([&]() {
        browser()
            ->tab_strip_model()
            ->GetActiveTab()
            ->lens_overlay_controller()
            ->ShowUI();
      }),
      // The overlay controller is an independent floating widget associated
      // with a tab rather than a browser window, so by convention gets its own
      // element context.
      InAnyContext(InstrumentNonTabWebView(kOverlayId,
                                           LensOverlayController::kOverlayId)),
      // Wait for the webview to finish loading to prevent re-entrancy.
      InSameContext(Steps(FlushEvents(),
                          EnsurePresent(kOverlayId, kPathToCloseButton),
                          ExecuteJsAt(kOverlayId, kPathToCloseButton, kClickFn,
                                      ExecuteJsMode::kFireAndForget),
                          WaitForHide(kOverlayId))));
}

}  // namespace
