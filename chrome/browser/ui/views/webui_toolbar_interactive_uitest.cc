// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

const WebContentsInteractionTestUtil::DeepQuery kReloadButtonDeepQuery = {
    "toolbar-app", "reload-button"};
}  // namespace

class WebUIToolbarViewsInteractiveUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebUIToolbarViewsInteractiveUiTest() {
    if (IsWebUIReloadButtonEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton,
           features::kSkipIPCChannelPausingForNonGuests,
           features::kWebUIInProcessResourceLoadingV2,
           features::kInitialWebUISyncNavStartToCommit},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIReloadButton,
               features::kSkipIPCChannelPausingForNonGuests,
               features::kWebUIInProcessResourceLoadingV2,
               features::kInitialWebUISyncNavStartToCommit});
    }
  }
  ~WebUIToolbarViewsInteractiveUiTest() override = default;

  bool IsWebUIReloadButtonEnabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Wait for the toolbar to load. Note that we can't wait for the widget to
    // become visible instead because the Widget will always be visible on Mac
    // OS.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->IsShowPending();
    }));
  }

  views::WebView* GetToolbarWebView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetWebUIToolbarViewForTesting()
        ->GetWebViewForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIToolbarViewsInteractiveUiTest,
                         testing::Bool());

// Test that the reload button exists, and clicking on it will cause the page to
// be reloaded.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest, ReloadPage) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, url),
      IsWebUIReloadButtonEnabled()
          ? Steps(
                InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
                WaitForJsResultAt(kWebUIToolbarId, kReloadButtonDeepQuery,
                                  "el => el.state.isNavigationLoading", false),
                MoveMouseTo(kWebUIToolbarId, kReloadButtonDeepQuery))
          : Steps(WaitForViewProperty(kReloadButtonElementId, ReloadButton,
                                      VisibleMode, ReloadButton::Mode::kReload),
                  MoveMouseTo(kReloadButtonElementId)),
      ClickMouse(), WaitForWebContentsNavigation(kTabId, url));
}
