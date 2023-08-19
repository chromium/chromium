// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

// Wait until a browser is removed from BrowserList.
class BrowserRemovalWaiter : public BrowserListObserver {
 public:
  explicit BrowserRemovalWaiter(const Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
  }
  BrowserRemovalWaiter(const BrowserRemovalWaiter&) = delete;
  BrowserRemovalWaiter& operator=(const BrowserRemovalWaiter&) = delete;
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

  const raw_ptr<const Browser, AcrossTasksDanglingUntriaged> browser_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
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
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  BrowserRemovalWaiter waiter(browser());
  tab_strip_model->SetTabPinned(0, false);
  int previous_tab_count = tab_strip_model->count();
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(previous_tab_count - 1, tab_strip_model->count());
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

  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  BrowserRemovalWaiter waiter(browser());
  browser()->window()->Close();
  waiter.WaitForRemoval();

  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("http://www.google.com/:pinned", result);
}
