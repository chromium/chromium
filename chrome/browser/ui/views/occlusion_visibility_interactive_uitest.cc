// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"

class VisibilityWaiter : public content::WebContentsObserver {
 public:
  VisibilityWaiter(content::WebContents* web_contents,
                   content::Visibility expected_visibility)
      : WebContentsObserver(web_contents),
        expected_visibility_(expected_visibility) {}

  void Wait() {
    if (web_contents()->GetVisibility() == expected_visibility_) {
      return;
    }
    run_loop_.Run();
  }

  // WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == expected_visibility_) {
      run_loop_.Quit();
    }
  }

 private:
  content::Visibility expected_visibility_;
  base::RunLoop run_loop_;
};

class OcclusionVisibilityInteractiveUITest : public InteractiveBrowserTest {
 public:
  OcclusionVisibilityInteractiveUITest() = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Occlusion is normally disabled for testing to prevent test flakiness,
    // but is enabled here because we're testing occlusion.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);
  }
};

// Verify that occluding a browser window triggers a WebContents visibility
// change.
// TODO(https://crbug.com/40122973): BringWindowToFront can get stuck in a loop
// on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OcclusionTriggersVisiblityChange \
  DISABLED_OcclusionTriggersVisiblityChange
#else
#define MAYBE_OcclusionTriggersVisiblityChange OcclusionTriggersVisiblityChange
#endif
IN_PROC_BROWSER_TEST_F(OcclusionVisibilityInteractiveUITest,
                       MAYBE_OcclusionTriggersVisiblityChange) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  browser()->window()->SetBounds(gfx::Rect(100, 100, 640, 480));
  browser()->GetWindow()->Show();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  Browser* browser2 = CreateBrowser(browser()->profile());

  {
    VisibilityWaiter waiter(web_contents, content::Visibility::OCCLUDED);
    gfx::Rect bounds = browser()->window()->GetBounds();
    bounds.Outset(50);
    browser2->window()->SetBounds(bounds);
    browser2->GetWindow()->Show();
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser2));
    waiter.Wait();
  }

  {
    VisibilityWaiter waiter(web_contents, content::Visibility::VISIBLE);
    browser2->window()->Close();
    waiter.Wait();
  }
}
