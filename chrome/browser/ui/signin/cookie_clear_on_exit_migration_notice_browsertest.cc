// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/cookie_clear_on_exit_migration_notice.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/signin/cookie_clear_on_exit_migration_notice.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr char kTestEmail[] = "email@gmail.com";

class BrowsersClosedObserver : public BrowserListObserver {
 public:
  explicit BrowsersClosedObserver(int closed_count)
      : closed_count_(closed_count) {
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }

  BrowsersClosedObserver(const BrowsersClosedObserver&) = delete;
  BrowsersClosedObserver& operator=(const BrowsersClosedObserver&) = delete;

  void Wait() { run_loop_.Run(); }

  void OnBrowserRemoved(Browser* browser) override {
    --closed_count_;
    if (closed_count_ == 0) {
      run_loop_.Quit();
    }
  }

 private:
  base::ScopedObservation<BrowserList, BrowsersClosedObserver>
      browser_list_observation_{this};
  int closed_count_ = 0;
  base::RunLoop run_loop_;
};

}  // namespace

class CookieClearOnExitMigrationNoticePixelTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    ShowCookieClearOnExitMigrationNotice(*browser(), base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticePixelTest,
                       InvokeUi_default) {
  Profile* profile = browser()->profile();
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile), "some@email.com",
      signin::ConsentLevel::kSignin);
  profile->GetPrefs()->ClearPref(
      prefs::kCookieClearOnExitMigrationNoticeComplete);

  ShowAndVerifyUi();

  profile->GetPrefs()->SetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete, true);
}

class CookieClearOnExitMigrationNoticeBrowserTest
    : public SigninBrowserTestBase {
 public:
  CookieClearOnExitMigrationNoticeBrowserTest() : SigninBrowserTestBase() {
    feature_list_.InitWithFeatureState(
        switches::kExplicitBrowserSigninUIOnDesktop,
        /*enabled=*/!content::IsPreTest());
  }

  AccountInfo SetPrimaryAccount(signin::ConsentLevel consent_level,
                                bool is_explicit_signin) {
    // `ACCESS_POINT_WEB_SIGNIN` is not explicit signin.
    signin_metrics::AccessPoint access_point =
        is_explicit_signin
            ? signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
            : signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN;
    signin::AccountAvailabilityOptionsBuilder builder =
        identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager(), builder.AsPrimary(consent_level)
                                .WithAccessPoint(access_point)
                                .WithCookie()
                                .Build(kTestEmail));
    CHECK_EQ(is_explicit_signin, GetProfile()->GetPrefs()->GetBoolean(
                                     prefs::kExplicitBrowserSignin));
    return account_info;
  }

  void SetGaiaCookieClearedOnExit(bool cleared) {
    ContentSetting setting =
        cleared ? CONTENT_SETTING_SESSION_ONLY : CONTENT_SETTING_ALLOW;
    content_settings::CookieSettings* settings =
        CookieSettingsFactory::GetForProfile(GetProfile()).get();
    settings->SetDefaultCookieSetting(setting);
    ASSERT_EQ(ChromeSigninClientFactory::GetForProfile(GetProfile())
                  ->AreSigninCookiesDeletedOnExit(),
              cleared);

    base::RunLoop loop;
    GetProfile()->GetPrefs()->CommitPendingWrite(loop.QuitClosure());
    loop.Run();
  }

  views::DialogDelegate* TryCloseBrowserAndWaitForNotice(Browser& browser) {
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "CookieClearOnExitMigrationNotice");

    CloseBrowserAsynchronously(&browser);

    // Confirmation prompt is shown.
    views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
    return confirmation_prompt->widget_delegate()->AsDialogDelegate();
  }

  views::DialogDelegate* TryCloseAllBrowsersAndWaitForNotice() {
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "CookieClearOnExitMigrationNotice");

    profiles::CloseProfileWindows(GetProfile());

    // Confirmation prompt is shown.
    views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
    return confirmation_prompt->widget_delegate()->AsDialogDelegate();
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();

    if (!content::IsPreTest()) {
      // Create a browser for another profile, so that Chrome does not exit in
      // the middle of the test.
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      base::FilePath other_profile_path =
          profile_manager->GenerateNextProfileDirectoryPath();
      Profile& other_profile = profiles::testing::CreateProfileSync(
          profile_manager, other_profile_path);
      CreateBrowser(&other_profile);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticeBrowserTest,
                       PRE_ShowNoticeCloseWindow) {
  SetGaiaCookieClearedOnExit(/*cleared=*/true);
}

// The notice is shown when the user is signed in and the user can close
// the window.
IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticeBrowserTest,
                       ShowNoticeCloseWindow) {
  ASSERT_TRUE(ChromeSigninClientFactory::GetForProfile(GetProfile())
                  ->AreSigninCookiesDeletedOnExit());

  SetPrimaryAccount(signin::ConsentLevel::kSignin,
                    /*is_explicit_signin=*/true);
  Browser* browser_2 = CreateBrowser(GetProfile());

  // No notice shown if there is another browser for this profile.
  CloseBrowserSynchronously(browser_2);

  ui_test_utils::BrowserChangeObserver browser_close_observer(
      browser(), ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);

  views::DialogDelegate* dialog_delegate =
      TryCloseBrowserAndWaitForNotice(*browser());
  EXPECT_TRUE(dialog_delegate);

  // User is not migrated yet.
  EXPECT_FALSE(GetProfile()->GetPrefs()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  // Click "Close window".
  dialog_delegate->AcceptDialog();

  // User is migrated and browser is closed.
  browser_close_observer.Wait();
  EXPECT_TRUE(GetProfile()->GetPrefs()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
}

IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticeBrowserTest,
                       PRE_ShowNoticeCancel) {
  SetGaiaCookieClearedOnExit(/*cleared=*/true);
  SetPrimaryAccount(signin::ConsentLevel::kSync,
                    /*is_explicit_signin=*/false);
}

// The notice is shown when the user is syncing when the feature is enabled, and
// the user can cancel.
IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticeBrowserTest,
                       ShowNoticeCancel) {
  ASSERT_TRUE(ChromeSigninClientFactory::GetForProfile(GetProfile())
                  ->AreSigninCookiesDeletedOnExit());
  // Turn off sync and signin with explicit consent.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  identity_test_env()->ClearPrimaryAccount();
  SetPrimaryAccount(signin::ConsentLevel::kSignin,
                    /*is_explicit_signin=*/true);

  views::DialogDelegate* dialog_delegate =
      TryCloseBrowserAndWaitForNotice(*browser());
  EXPECT_TRUE(dialog_delegate);

  // User is not migrated yet.
  EXPECT_FALSE(GetProfile()->GetPrefs()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  // Click "Cancel".
  dialog_delegate->CancelDialog();

  // User is migrated, and browser is not closed.
  EXPECT_TRUE(GetProfile()->GetPrefs()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  EXPECT_FALSE(browser()->IsAttemptingToCloseBrowser());
  EXPECT_FALSE(browser()->IsBrowserClosing());

  // The browser can now be closed normally.
  CloseBrowserSynchronously(browser());
}

IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticeBrowserTest,
                       PRE_ShowNoticeMultipleWindows) {
  SetGaiaCookieClearedOnExit(/*cleared=*/true);
}

// The notice is shown when the user is signed in and the user can close
// the window.
IN_PROC_BROWSER_TEST_F(CookieClearOnExitMigrationNoticeBrowserTest,
                       ShowNoticeMultipleWindows) {
  ASSERT_TRUE(ChromeSigninClientFactory::GetForProfile(GetProfile())
                  ->AreSigninCookiesDeletedOnExit());

  SetPrimaryAccount(signin::ConsentLevel::kSignin,
                    /*is_explicit_signin=*/true);

  // Create multiple windows.
  CreateBrowser(GetProfile());
  CreateBrowser(GetProfile());

  // Try closing all windows at once. 2 should close immediately, the remaining
  // one displays the notice.
  BrowsersClosedObserver browsers_closed_observer(2);
  views::DialogDelegate* dialog_delegate =
      TryCloseAllBrowsersAndWaitForNotice();
  EXPECT_TRUE(dialog_delegate);
  browsers_closed_observer.Wait();

  // User is not migrated yet.
  EXPECT_FALSE(GetProfile()->GetPrefs()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  BrowsersClosedObserver last_browser_closed_observer(1);
  dialog_delegate->AcceptDialog();

  // User is migrated and browser is closed.
  last_browser_closed_observer.Wait();
  EXPECT_TRUE(GetProfile()->GetPrefs()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
}
