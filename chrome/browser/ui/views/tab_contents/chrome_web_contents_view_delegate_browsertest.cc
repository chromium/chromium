// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#endif

class ChromeWebContentsViewDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ChromeWebContentsViewDelegateBrowserTest() = default;
  ChromeWebContentsViewDelegateBrowserTest(
      const ChromeWebContentsViewDelegateBrowserTest&) = delete;
  ChromeWebContentsViewDelegateBrowserTest& operator=(
      const ChromeWebContentsViewDelegateBrowserTest&) = delete;
  ~ChromeWebContentsViewDelegateBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeatures({features::kSideBySide}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ChromeWebContentsViewDelegateBrowserTest,
                       ContextMenuFocusesWebContents) {
  // Add a second tab and create split view.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  // Ensure focus is on the left tab.
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);

  // Right click the right tab.
  std::unique_ptr<content::WebContentsViewDelegate> delegate_view =
      CreateWebContentsViewDelegate(
          browser()->tab_strip_model()->GetWebContentsAt(1));
  const content::ContextMenuParams params;
  delegate_view->ShowContextMenu(
      *browser()->tab_strip_model()->GetWebContentsAt(1)->GetPrimaryMainFrame(),
      params);

  // Ensure focus moves to the other tab.
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
}
#endif
