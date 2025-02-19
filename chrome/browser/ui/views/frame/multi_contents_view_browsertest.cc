// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);

class MultiContentsViewBrowserTest : public InteractiveBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that MultiContentsView exists when the side by side flag is enabled
IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, ExistsWithFlag) {
  RunTestSequence(
      EnsurePresent(MultiContentsView::kMultiContentsViewElementId));
}

// Check that MultiContentsView executes its callback on inactive view mouse
// down.
IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, ActivatesInactiveView) {
  RunTestSequence(
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUISettingsURL), 0),
      WaitForWebContentsReady(kNewTab),
      Check([=, this]() { return tab_strip_model()->count() == 2u; }),
      Check([=, this]() { return tab_strip_model()->active_index() == 0; }),
      Do([&]() {
        content::WebContents* inactive_contents =
            tab_strip_model()->GetWebContentsAt(1);
        browser_view()->multi_contents_view_for_testing()->SetWebContents(
            inactive_contents, false);

        // Simulate a mouse click event on the inactive contents, which should
        // trigger the activation callback.
        content::SimulateMouseClick(inactive_contents, 0,
                                    blink::WebPointerProperties::Button::kLeft);
      }),
      Check([&]() { return tab_strip_model()->active_index() == 1; }));
}
