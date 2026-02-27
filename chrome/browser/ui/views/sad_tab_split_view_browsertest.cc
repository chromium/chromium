// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/sad_tab.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

using SadTabSplitViewBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SadTabSplitViewBrowserTest,
                       SadTabMovedToSecondarySplitView) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  // Add a second tab. Tab 0 is NTP, Tab 1 is about:blank.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, GURL("about:blank"),
                                     ui::PAGE_TRANSITION_LINK, false));

  // Crash the second tab (Tab 1).
  content::WebContents* crash_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::RenderProcessHost* process =
      crash_contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  crash_observer.Wait();

  // Verify Sad Tab is created.
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(crash_contents);
  ASSERT_TRUE(sad_tab_helper);
  ASSERT_TRUE(sad_tab_helper->sad_tab());

  // Activate Tab 0 (the healthy one).
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Enter split view with Tab 0 as primary and Tab 1 (crashed) as secondary.
  browser()->tab_strip_model()->AddToNewSplit(
      {1},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                     1.0f),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Verify split view is active.
  EXPECT_TRUE(browser()->tab_strip_model()->IsActiveTabSplit());

  // Find SadTabView among the visible ContentsWebViews.
  SadTabView* sad_tab_view = nullptr;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  for (auto* web_view : browser_view->GetAllVisibleContentsWebViews()) {
    for (views::View* child : web_view->children()) {
      if (views::IsViewClass<SadTabView>(child)) {
        sad_tab_view = static_cast<SadTabView*>(child);
        // Verify this web view actually holds the crashed contents
        EXPECT_EQ(web_view->web_contents(), crash_contents);
        break;
      }
    }
    if (sad_tab_view) {
      break;
    }
  }

  ASSERT_TRUE(sad_tab_view)
      << "SadTabView is not attached to any visible ContentsWebView";
  EXPECT_TRUE(sad_tab_view->GetWidget());
  EXPECT_TRUE(sad_tab_view->IsDrawn());
}

}  // namespace
