// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#endif

namespace {

// Returns true if signin happens in a modal dialog and false if it happens in a
// regular tab.
bool IsSigninModal(Profile* profile) {
#if defined(OS_CHROMEOS)
  return false;
#else
  return AccountConsistencyModeManager::GetMethodForProfile(profile) ==
         signin::AccountConsistencyMethod::kDisabled;
#endif
}

// Returns whether the signin modal dialog is displayed.
bool ShowsModalDialog(Browser* browser) {
#if defined(OS_CHROMEOS)
  NOTREACHED();
  return false;
#else
  return browser->signin_view_controller()->ShowsModalDialog();
#endif
}

}  // namespace

class BookmarkBubbleSignInDelegateTest : public InProcessBrowserTest {
 public:
  BookmarkBubbleSignInDelegateTest() {}

  Profile* profile() { return browser()->profile(); }

  void ReplaceBlank(Browser* browser);

  void SignInBrowser(Browser* browser);

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleSignInDelegateTest);
};

// The default browser created for tests start with one tab open on
// about:blank.  The sign-in page is a singleton that will
// replace this tab.  This function replaces about:blank with another URL
// so that the sign in page goes into a new tab.
void BookmarkBubbleSignInDelegateTest::ReplaceBlank(Browser* browser) {
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL("chrome:version")));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

void BookmarkBubbleSignInDelegateTest::SignInBrowser(Browser* browser) {
  std::unique_ptr<BubbleSyncPromoDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser));
  delegate->OnEnableSync(AccountInfo(), false /* is_default_promo_account */);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest, OnSignInLinkClicked) {
  ReplaceBlank(browser());
  int starting_tab_count = browser()->tab_strip_model()->count();
  SignInBrowser(browser());

  if (IsSigninModal(profile())) {
    EXPECT_TRUE(ShowsModalDialog(browser()));
    EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());
  } else {
    EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  }
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest,
                       OnSignInLinkClickedReusesBlank) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  SignInBrowser(browser());

  if (IsSigninModal(profile())) {
    EXPECT_TRUE(ShowsModalDialog(browser()));
  }
  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest,
                       OnSignInLinkClickedIncognito_RegularBrowserWithTabs) {
  ReplaceBlank(browser());
  int starting_tab_count = browser()->tab_strip_model()->count();
  EXPECT_GT(starting_tab_count, 0);
  Browser* incognito_browser = CreateIncognitoBrowser();
  int starting_tab_count_incognito =
      incognito_browser->tab_strip_model()->count();

  SignInBrowser(incognito_browser);

  int tab_count = browser()->tab_strip_model()->count();
  if (IsSigninModal(profile())) {
#if !defined(OS_CHROMEOS)
    // ProfileChooser doesn't show in an incognito window.
    EXPECT_FALSE(ProfileMenuView::IsShowing());
#endif

    // Sign-in dialog is shown when there is at least one tab in the
    // non-incognito browser.
    EXPECT_EQ(starting_tab_count, tab_count);
    EXPECT_TRUE(ShowsModalDialog(browser()));
  } else {
    // On ChromeOS, the full-tab signin page is used.
    EXPECT_EQ(starting_tab_count + 1, tab_count);
  }

  // No effect is expected on the incognito browser.
  int tab_count_incognito = incognito_browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count_incognito, tab_count_incognito);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest,
                       OnSignInLinkClickedIncognito_RegularBrowserClosed) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  int starting_tab_count_incognito =
      incognito_browser->tab_strip_model()->count();
  // Close the main browser.
  CloseBrowserSynchronously(browser());

  SignInBrowser(incognito_browser);

  // Signing in fom incognito should create a new non-incognito browser.
  Browser* new_regular_browser = chrome::FindTabbedBrowser(
      incognito_browser->profile()->GetOriginalProfile(), false);
  if (IsSigninModal(new_regular_browser->profile())) {
    EXPECT_FALSE(ShowsModalDialog(new_regular_browser));
  }

  // The full-tab sign-in page should be shown in the newly created browser.
  EXPECT_EQ(1, new_regular_browser->tab_strip_model()->count());

  // No effect is expected on the incognito browser.
  int tab_count_incognito = incognito_browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count_incognito, tab_count_incognito);
}

// Verifies that the sign in page can be loaded in a different browser
// if the provided browser is invalidated.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest, BrowserRemoved) {
  // Create an extra browser.
  Browser* extra_browser = CreateBrowser(profile());
  ReplaceBlank(extra_browser);

  int starting_tab_count = extra_browser->tab_strip_model()->count();

  std::unique_ptr<BubbleSyncPromoDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  BrowserList::SetLastActive(extra_browser);

  // Close all tabs in the original browser.  Run all pending messages
  // to make sure the browser window closes before continuing.
  browser()->tab_strip_model()->CloseAllTabs();
  content::RunAllPendingInMessageLoop();

  delegate->OnEnableSync(AccountInfo(), false /* is_default_promo_account */);

  int tab_count = extra_browser->tab_strip_model()->count();
  if (IsSigninModal(extra_browser->profile())) {
    EXPECT_TRUE(ShowsModalDialog(extra_browser));
    EXPECT_EQ(starting_tab_count, tab_count);
  } else {
    // A new tab should have been opened in the extra browser, which should be
    // visible.
    EXPECT_EQ(starting_tab_count + 1, tab_count);
  }
}
