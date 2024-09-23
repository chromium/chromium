// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_view_controller.h"

#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/logout_tab_helper.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr char kTestEmail[] = "email@gmail.com";
constexpr signin_metrics::AccessPoint kTestAccessPoint = signin_metrics::
    AccessPoint::ACCESS_POINT_PROFILE_MENU_SIGNOUT_CONFIRMATION_PROMPT;

constexpr char kConfirmationNoUnsyncedHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.NoUnsynced";
constexpr char kConfirmationUnsyncedHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.Unsynced";
constexpr char kConfirmationUnsyncedReauthHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.UnsyncedReauth";
constexpr std::string_view kTestExtensionName = "Test extension";

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

void VerifySignoutPromptHistogram(
    const base::HistogramTester& histogram_tester,
    ChromeSignoutConfirmationPromptVariant variant,
    ChromeSignoutConfirmationChoice choice) {
  const char* histogram_name = kConfirmationUnsyncedHistogramName;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      histogram_name = kConfirmationNoUnsyncedHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      histogram_name = kConfirmationUnsyncedReauthHistogramName;
      break;
  }

  histogram_tester.ExpectUniqueSample(histogram_name, choice, 1);
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[histogram_name] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Signin.ChromeSignoutConfirmationPrompt."),
              testing::ContainerEq(expected_counts));
}

}  // namespace

class SigninViewControllerBrowserTestBase : public SigninBrowserTestBase {
 public:
  SigninViewControllerBrowserTestBase() = default;

  AccountInfo SetPrimaryAccount() {
    return identity_test_env()->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
  }

  void AddUnsyncedData() {
    GetTestSyncService()->SetTypesWithUnsyncedData(
        syncer::DataTypeSet{syncer::DataType::PASSWORDS});
  }

  views::DialogDelegate* TriggerSignoutAndWaitForConfirmationPrompt() {
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "ChromeSignoutConfirmationChoicePrompt");
    browser()->signin_view_controller()->SignoutOrReauthWithPrompt(
        kTestAccessPoint,
        signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
        signin_metrics::SourceForRefreshTokenOperation::
            kUserMenu_SignOutAllAccounts);

    // Confirmation prompt is shown.
    views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
    return confirmation_prompt->widget_delegate()->AsDialogDelegate();
  }

  bool IsSigninTab(content::WebContents* tab) const {
    DiceTabHelper* dice_tab_helper = DiceTabHelper::FromWebContents(tab);
    if (!dice_tab_helper) {
      return false;
    }

    if (!dice_tab_helper->IsChromeSigninPage()) {
      ADD_FAILURE();
      return false;
    }
    if (dice_tab_helper->signin_access_point() != kTestAccessPoint) {
      ADD_FAILURE();
      return false;
    }
    return true;
  }

  bool IsSignoutTab(content::WebContents* tab) const {
    return LogoutTabHelper::FromWebContents(tab);
  }

 private:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }
};

class SigninViewControllerBrowserImplicitSigninTest
    : public SigninViewControllerBrowserTestBase {
 public:
  SigninViewControllerBrowserImplicitSigninTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserImplicitSigninTest,
                       NoPrompt) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();

  // Add pending sync data.
  AddUnsyncedData();

  // Trigger the Chrome signout action.
  browser()->signin_view_controller()->SignoutOrReauthWithPrompt(
      kTestAccessPoint,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);

  // Sign out tab opens immediately. The user may not be signed out yet, as the
  // sign out happens through the web.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

class SigninViewControllerBrowserTest
    : public SigninViewControllerBrowserTestBase {
 public:
  SigninViewControllerBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kExplicitBrowserSigninUIOnDesktop,
         switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

  views::DialogDelegate* TriggerChromeSigninDialogForExtensionsPrompt(
      base::OnceClosure on_complete) {
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "ChromeSigninChoiceForExtensionsPrompt");
    browser()
        ->signin_view_controller()
        ->MaybeShowChromeSigninDialogForExtensions(kTestExtensionName,
                                                   std::move(on_complete));

    // Confirmation prompt is shown.
    views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
    return confirmation_prompt->widget_delegate()->AsDialogDelegate();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_Reauth) {
  // Setup a primary account in error state.
  AccountInfo primary_account_info = SetPrimaryAccount();
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Add pending sync data.
  AddUnsyncedData();

  // Trigger the Chrome signout action.
  views::DialogDelegate* dialog_delegate =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(dialog_delegate);

  // Click "Verify it's you".
  base::HistogramTester histogram_tester;
  dialog_delegate->AcceptDialog();
  VerifySignoutPromptHistogram(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton,
      ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth);

  // The tab was navigated to the signin page.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSigninTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_Cancel) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Add pending sync data.
  AddUnsyncedData();

  // Trigger the Chrome signout action.
  views::DialogDelegate* dialog_delegate =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(dialog_delegate);

  // Click "Cancel".
  base::HistogramTester histogram_tester;
  dialog_delegate->CancelDialog();
  VerifySignoutPromptHistogram(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
      ChromeSignoutConfirmationChoice::kCancelSignout);

  // User is still signed in.
  EXPECT_EQ(
      primary_account_info.account_id,
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  // The tab was not navigated to the signin page or signout page.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_FALSE(IsSigninTab(tab));
  EXPECT_FALSE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_SignOutWithUnsyncedData) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Add pending sync data.
  AddUnsyncedData();

  // Trigger the Chrome signout action.
  views::DialogDelegate* dialog_delegate =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(dialog_delegate);

  // Click "Sign Out Anyway".
  base::HistogramTester histogram_tester;
  dialog_delegate->AcceptDialog();
  VerifySignoutPromptHistogram(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
      ChromeSignoutConfirmationChoice::kSignout);

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_SignOut) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Trigger the Chrome signout action.
  views::DialogDelegate* dialog_delegate =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(dialog_delegate);

  // Click "Sign Out Anyway".
  base::HistogramTester histogram_tester;
  dialog_delegate->AcceptDialog();
  VerifySignoutPromptHistogram(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
      ChromeSignoutConfirmationChoice::kSignout);

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_NoPrompt) {
  // Setup a primary account in auth error.
  AccountInfo primary_account_info = SetPrimaryAccount();
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Trigger the Chrome signout action.
  browser()->signin_view_controller()->SignoutOrReauthWithPrompt(
      kTestAccessPoint,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninDialogForExtensionsPromptReuseOpenTab) {
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_TRUE(SigninViewController::IsNTPTab(
      browser()->tab_strip_model()->GetActiveWebContents()));

  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(SigninViewController::IsNTPTab(tab));

  ASSERT_FALSE(future.IsReady());
  dialog_delegate->AcceptDialog();

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    ShowChromeSigninDialogForExtensionsPromptReuseInactiveOpenTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  ASSERT_FALSE(SigninViewController::IsNTPTab(
      browser()->tab_strip_model()->GetActiveWebContents()));

  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(SigninViewController::IsNTPTab(tab));

  ASSERT_FALSE(future.IsReady());
  dialog_delegate->AcceptDialog();

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninDialogForExtensionsPromptInNewTab) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  ASSERT_FALSE(SigninViewController::IsNTPTab(
      browser()->tab_strip_model()->GetActiveWebContents()));

  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(SigninViewController::IsNTPTab(tab));

  ASSERT_FALSE(future.IsReady());
  dialog_delegate->AcceptDialog();

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninDialogForExtensionsPromptCancel) {
  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);

  ASSERT_FALSE(future.IsReady());
  dialog_delegate->CancelDialog();

  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    ShowChromeSigninDialogForExtensionsPromptNotShownPrimaryAccountSet) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  browser()->signin_view_controller()->MaybeShowChromeSigninDialogForExtensions(
      kTestExtensionName, future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    ShowChromeSigninDialogForExtensionsPromptNotShownNoAccounts) {
  base::test::TestFuture<void> future;
  browser()->signin_view_controller()->MaybeShowChromeSigninDialogForExtensions(
      kTestExtensionName, future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

class SigninViewControllerBrowserCookieParamTest
    : public SigninViewControllerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool with_cookies() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(SigninViewControllerBrowserCookieParamTest, SignOut) {
  // Setup a primary account, and cookie if requested.
  AccountInfo primary_account_info = SetPrimaryAccount();
  if (with_cookies()) {
    identity_test_env()->SetCookieAccounts(
        {{.email = kTestEmail,
          .gaia_id = signin::GetTestGaiaIdForEmail(kTestEmail),
          .signed_out = false}});
  }
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(true);
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Trigger the Chrome signout action, and confirm the prompt.
  views::DialogDelegate* dialog_delegate =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(dialog_delegate);
  dialog_delegate->AcceptDialog();

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Signout tab was opened only if cookies there were cookies for the account.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(IsSignoutTab(tab), with_cookies());
  EXPECT_FALSE(IsSigninTab(tab));
}

INSTANTIATE_TEST_SUITE_P(,
                         SigninViewControllerBrowserCookieParamTest,
                         testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithCookie" : "NoCookie";
                         });
