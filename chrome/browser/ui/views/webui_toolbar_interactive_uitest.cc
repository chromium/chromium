// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
}  // namespace

class WebUIToolbarWebViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  WebUIToolbarWebViewInteractiveUiTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }
  ~WebUIToolbarWebViewInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Wait for the toolbar to load. Note that we can't wait for the widget to
    // become visible instead because the Widget will always be visible on Mac
    // OS.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->ShouldDeferShow();
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the reload button exists, and clicking on it will cause the page to
// be reloaded.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveUiTest, ReloadPage) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      // Initial navigation to `url`.
      InstrumentTab(kTabId), NavigateWebContents(kTabId, url),
      // Trigger the reload.
      MoveMouseTo(kReloadButtonElementId,
                  base::BindOnce([](ui::TrackedElement* el) {
                    return el->GetScreenBounds().CenterPoint();
                  })),
      ClickMouse(),
      // There should be another navigation to the same `url`.
      WaitForWebContentsNavigation(kTabId, url));
}
