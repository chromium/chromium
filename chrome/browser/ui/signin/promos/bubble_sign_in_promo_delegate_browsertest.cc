// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"

class BubbleSignInPromoDelegateTest : public InProcessBrowserTest {
 public:
  BubbleSignInPromoDelegateTest() = default;

  BubbleSignInPromoDelegateTest(const BubbleSignInPromoDelegateTest&) = delete;
  BubbleSignInPromoDelegateTest& operator=(
      const BubbleSignInPromoDelegateTest&) = delete;

  Profile* profile() { return browser()->GetProfile(); }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  void ReplaceBlank(BrowserWindowInterface* browser);

  void SignInBrowser(BrowserWindowInterface* browser);
};

// The default browser created for tests start with one tab open on
// about:blank.  The sign-in page is a singleton that will
// replace this tab.  This function replaces about:blank with another URL
// so that the sign in page goes into a new tab.
void BubbleSignInPromoDelegateTest::ReplaceBlank(
    BrowserWindowInterface* browser) {
  ShowSingletonTabOverwritingNTP(browser, GURL("chrome:version"),
                                 NavigateParams::IGNORE_AND_NAVIGATE);
}

void BubbleSignInPromoDelegateTest::SignInBrowser(
    BrowserWindowInterface* browser) {
  auto delegate =
      std::make_unique<BubbleSignInPromoForSyncableDataTypeDelegate>(
          *browser->GetTabStripModel()->GetActiveWebContents(),
          signin_metrics::AccessPoint::kBookmarkBubble,
          syncer::LocalDataItemModel::DataId());
  delegate->OnSignIn(AccountInfo());
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest, OnSignInLinkClicked) {
  ReplaceBlank(browser());
  int starting_tab_count = browser()->GetTabStripModel()->count();
  SignInBrowser(browser());
  EXPECT_EQ(starting_tab_count + 1, browser()->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       OnSignInLinkClickedReusesBlank) {
  int starting_tab_count = browser()->GetTabStripModel()->count();
  SignInBrowser(browser());
  EXPECT_EQ(starting_tab_count, browser()->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       OnSignInLinkClickedIncognito_RegularBrowserWithTabs) {
  ReplaceBlank(browser());
  int starting_tab_count = browser()->GetTabStripModel()->count();
  EXPECT_GT(starting_tab_count, 0);
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  int starting_tab_count_incognito =
      incognito_browser->GetTabStripModel()->count();

  SignInBrowser(incognito_browser);

  int tab_count = browser()->GetTabStripModel()->count();
  // A full-tab signin page is used.
  EXPECT_EQ(starting_tab_count + 1, tab_count);

  // No effect is expected on the incognito browser.
  int tab_count_incognito = incognito_browser->GetTabStripModel()->count();
  EXPECT_EQ(starting_tab_count_incognito, tab_count_incognito);
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       OnSignInLinkClickedIncognito_RegularBrowserClosed) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  int starting_tab_count_incognito =
      incognito_browser->GetTabStripModel()->count();
  // Close the main browser.
  CloseBrowserSynchronously(browser());

  SignInBrowser(incognito_browser);

  // Signing in fom incognito should create a new non-incognito browser.
  BrowserWindowInterface* new_regular_browser =
      ProfileBrowserCollection::GetForProfile(incognito_browser->GetProfile())
          ->FindTabbedBrowser(/*match_original_profiles=*/true);

  // The full-tab sign-in page should be shown in the newly created browser.
  EXPECT_EQ(1, new_regular_browser->GetTabStripModel()->count());

  // No effect is expected on the incognito browser.
  int tab_count_incognito = incognito_browser->GetTabStripModel()->count();
  EXPECT_EQ(starting_tab_count_incognito, tab_count_incognito);
}

// Verifies that the sign in page can be loaded in a different browser
// if the provided browser is invalidated.
IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest, BrowserRemoved) {
  // Create an extra browser.
  BrowserWindowInterface* extra_browser = CreateBrowser(profile());
  ReplaceBlank(extra_browser);

  int starting_tab_count = extra_browser->GetTabStripModel()->count();

  std::unique_ptr<BubbleSignInPromoDelegate> delegate =
      std::make_unique<BubbleSignInPromoForSyncableDataTypeDelegate>(
          *extra_browser->GetTabStripModel()->GetActiveWebContents(),
          signin_metrics::AccessPoint::kBookmarkBubble,
          syncer::LocalDataItemModel::DataId());

  ui_test_utils::DeprecatedFakeActivateBrowser(extra_browser);

  // Close all tabs in the original browser.  Run all pending messages
  // to make sure the browser window closes before continuing.
  browser()->GetTabStripModel()->CloseAllTabs();
  content::RunAllPendingInMessageLoop();

  delegate->OnSignIn(AccountInfo());

  int tab_count = extra_browser->GetTabStripModel()->count();
  // A new tab should have been opened in the extra browser, which should be
  // visible.
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       CallbackDelegateAlreadySignedIn) {
  // Simulate synchronous sign-in state by already having the primary account
  // set.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<void> future;

  DefaultBubbleSignInPromoDelegate delegate(
      *browser()->GetTabStripModel()->GetActiveWebContents(),
      signin_metrics::AccessPoint::kSendTabToSelfPromo, future.GetCallback());

  delegate.OnSignIn(info);

  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       CallbackDelegateAsyncSignIn) {
  ReplaceBlank(browser());

  base::test::TestFuture<void> future;

  DefaultBubbleSignInPromoDelegate delegate(
      *browser()->GetTabStripModel()->GetActiveWebContents(),
      signin_metrics::AccessPoint::kSendTabToSelfPromo, future.GetCallback());

  delegate.OnSignIn(AccountInfo());

  // The delegate should open a sign-in tab and register the callback on its
  // helper.
  content::WebContents* sign_in_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::kSendTabToSelfPromo);
  ASSERT_TRUE(sign_in_tab);

  EXPECT_FALSE(future.IsReady());

  // Now simulate successful sign-in in that tab to fire the callback.
  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kSendTabToSelfPromo)
          .Build("test@gmail.com"));

  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       CallbackDelegateReauthSignIn) {
  ReplaceBlank(browser());

  // 1. Simulate sign-in pending state (has primary account but with error).
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
  ASSERT_TRUE(signin_util::IsSigninPending(identity_manager()));

  base::test::TestFuture<void> future;

  DefaultBubbleSignInPromoDelegate delegate(
      *browser()->GetTabStripModel()->GetActiveWebContents(),
      signin_metrics::AccessPoint::kSendTabToSelfPromo, future.GetCallback());

  // 2. Trigger OnSignIn. This should open the reauth tab.
  delegate.OnSignIn(info);

  // Verify the callback hasn't fired yet because the user hasn't
  // reauthenticated.
  EXPECT_FALSE(future.IsReady());

  // 3. Now successfully reauthenticate.
  // We need to simulate the reauth event with the correct access point.
  AccountInfo extended_info =
      AccountInfo::Builder(identity_manager()->FindExtendedAccountInfo(info))
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSendTabToSelfPromo)
          .Build();
  signin::UpdateAccountInfoForAccount(identity_manager(), extended_info);

  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.GetAccountId(),
      GoogleServiceAuthError::AuthErrorNone());

  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(BubbleSignInPromoDelegateTest,
                       CallbackDelegateTabClosedBeforeSignIn) {
  ReplaceBlank(browser());

  base::test::TestFuture<void> future;

  DefaultBubbleSignInPromoDelegate delegate(
      *browser()->GetTabStripModel()->GetActiveWebContents(),
      signin_metrics::AccessPoint::kSendTabToSelfPromo, future.GetCallback());

  delegate.OnSignIn(AccountInfo());

  // Find and close the newly opened sign-in tab.
  content::WebContents* sign_in_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::kSendTabToSelfPromo);
  ASSERT_TRUE(sign_in_tab);

  content::WebContentsDestroyedWatcher watcher(sign_in_tab);
  browser()->GetTabStripModel()->CloseWebContents(
      sign_in_tab, TabCloseTypes::CLOSE_USER_GESTURE);
  watcher.Wait();

  // The future should NOT be ready since the tab was closed without sign-in.
  EXPECT_FALSE(future.IsReady());

  // Use the matching access point to verify the observer is fully detached,
  // rather than relying on the helper's internal access point filtering to
  // ignore the event.
  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kSendTabToSelfPromo)
          .Build("another@gmail.com"));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}
