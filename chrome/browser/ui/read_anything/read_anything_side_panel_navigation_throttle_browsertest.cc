// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
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
          content::WebContents::CreateParams(browser()->profile()));

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
