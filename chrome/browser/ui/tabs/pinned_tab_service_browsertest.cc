// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

namespace {

// Wait until a browser is removed from BrowserList.
class BrowserRemovalWaiter : public BrowserListObserver {
 public:
  explicit BrowserRemovalWaiter(const Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
  }
  ~BrowserRemovalWaiter() override = default;

  void WaitForRemoval() {
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  // BrowserListObserver override:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser != browser_)
      return;

    BrowserList::RemoveObserver(this);
    if (message_loop_runner_.get() && message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  const Browser* const browser_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalWaiter);
};

using PinnedTabServiceBrowserTest = InProcessBrowserTest;

}  // namespace

// Makes sure pinned tabs are updated when tabstrip is empty.
// http://crbug.com/71939
IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, TabStripEmpty) {
  Profile* profile = browser()->profile();
  GURL url("http://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(profile);
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("http://www.google.com/:pinned", result);

  // When tab strip is empty, browser window will be closed and PinnedTabService
  // must update data on this event.
  BrowserRemovalWaiter waiter(browser());
  tab_strip_model->SetTabPinned(0, false);
  EXPECT_TRUE(
      tab_strip_model->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE));
  EXPECT_TRUE(tab_strip_model->empty());
  waiter.WaitForRemoval();

  // Let's see it's cleared out properly.
  result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, CloseWindow) {
  Profile* profile = browser()->profile();
  EXPECT_TRUE(PinnedTabServiceFactory::GetForProfile(profile));
  EXPECT_TRUE(profile->GetPrefs());

  GURL url("http://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  BrowserRemovalWaiter waiter(browser());
  browser()->window()->Close();
  waiter.WaitForRemoval();

  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("http://www.google.com/:pinned", result);
}
