// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class ReadAnythingSidePanelNavigationThrottleBrowserTest
    : public InProcessBrowserTest {
 public:
  ReadAnythingSidePanelNavigationThrottleBrowserTest() = default;
};

// Regression test for crbug.com/481276968.
// Verifies that navigating to the Read Anything Side Panel URL in a non-tab
// WebContents does not crash the browser.
IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelNavigationThrottleBrowserTest,
                       NavigateInNonTabWebContentsDoesNotCrash) {
  // Create a standalone WebContents (not associated with a tab).
  std::unique_ptr<content::WebContents> standalone_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));

  // Navigate to the Read Anything Side Panel URL with PAGE_TRANSITION_TYPED.
  content::NavigationController::LoadURLParams params{
      GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)};
  params.transition_type = ui::PAGE_TRANSITION_TYPED;

  content::TestNavigationObserver observer(standalone_web_contents.get());
  standalone_web_contents->GetController().LoadURLWithParams(params);
  observer.Wait();

  // The navigation should have been cancelled by the throttle.
  EXPECT_FALSE(observer.last_navigation_succeeded());

  // The key verification is that the test completes without crashing.
}

// Regression test for crbug.com/382399955.
// Verifies that navigating Reading Mode's own WebContents to the Reading Mode
// URL with PAGE_TRANSITION_TYPED (e.g. DevTools "Record and reload") proceeds.
IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelNavigationThrottleBrowserTest,
                       NavigateInReadAnythingWebContentsProceeds) {
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(tab);
  auto* controller = ReadAnythingController::From(tab);
  ASSERT_TRUE(controller);

  ReadAnythingContentsWrapper wrapper = controller->GetOrCreateWebUIWrapper(
      ReadAnythingController::PresentationState::kInactive);
  content::WebContents* ra_web_contents = wrapper->web_contents();
  ASSERT_TRUE(ra_web_contents);
  content::WaitForLoadStop(ra_web_contents);

  // Navigate to about:blank first, then back to the Reading Mode URL.
  {
    content::NavigationController::LoadURLParams blank_params{
        GURL(url::kAboutBlankURL)};
    blank_params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API);
    content::TestNavigationObserver blank_observer(ra_web_contents);
    ra_web_contents->GetController().LoadURLWithParams(blank_params);
    blank_observer.Wait();
    ASSERT_TRUE(blank_observer.last_navigation_succeeded());
    EXPECT_EQ(ra_web_contents->GetLastCommittedURL(),
              GURL(url::kAboutBlankURL));
  }

  {
    content::NavigationController::LoadURLParams reload_params{
        GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)};
    reload_params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API);
    content::TestNavigationObserver reload_observer(ra_web_contents);
    ra_web_contents->GetController().LoadURLWithParams(reload_params);
    reload_observer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(ra_web_contents->GetLastCommittedURL(),
              GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL));
  }
}
