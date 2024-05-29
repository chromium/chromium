// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"
#import "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#import "testing/gtest_mac.h"

using WebContentsViewMacInteractiveTest = InProcessBrowserTest;

namespace {

// A helper that will wait until a tab is removed from a specific Browser.
class TabRemovedWaiter : public TabStripModelObserver {
 public:
  explicit TabRemovedWaiter(Browser* browser) {
    browser->tab_strip_model()->AddObserver(this);
  }
  TabRemovedWaiter(const TabRemovedWaiter&) = delete;
  TabRemovedWaiter& operator=(const TabRemovedWaiter&) = delete;
  ~TabRemovedWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kRemoved)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// Integration test for a <select> popup run by the Mac-specific
// content::PopupMenuHelper, owned by a WebContentsViewMac.
// TODO(crbug.com/40758190): this test is flaking on the bots. Re-enable it when
// it's fixed.
IN_PROC_BROWSER_TEST_F(WebContentsViewMacInteractiveTest,
                       DISABLED_SelectMenuLifetime) {
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(embedded_test_server()->GetURL("/select.html")),
                    ui::PAGE_TRANSITION_LINK));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  __block NSString* first_item;

  // Set up a callback to trigger when a native menu is displayed.
  id token = [NSNotificationCenter.defaultCenter
      addObserverForName:NSMenuDidBeginTrackingNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                first_item = [[[notification.object itemAtIndex:0] title] copy];
                // We can't close the tab until after
                // NSMenuDidBeginTrackingNotification is processed (i.e. after
                // this block returns). So post a task to run on the inner run
                // loop which will close the tab (and cancel tracking in
                // ~PopupMenuHelper()) and quit the outer run loop to continue
                // the test.
                base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE, base::BindLambdaForTesting([&] {
                      browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
                    }));
              }];

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  TabRemovedWaiter tab_removed_waiter(browser());

  // Trigger the <select> menu via a simulated click, and wait for the ensuing
  // callback/observer/block/posted task magic to unfold.
  content::SimulateMouseClickOrTapElementWithId(web_contents, "select");
  tab_removed_waiter.Wait();

  [NSNotificationCenter.defaultCenter removeObserver:token];

  // Expect that the menu is no longer being tracked.
  EXPECT_NE(NSEventTrackingRunLoopMode, NSRunLoop.currentRunLoop.currentMode);
  // Expect that the menu that was shown was the expected one.
  EXPECT_NSEQ(@"Apple", first_item);
  // Expect that the browser tab is no longer there.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}
