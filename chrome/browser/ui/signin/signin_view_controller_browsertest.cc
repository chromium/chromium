// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_view_controller.h"

#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/logout_tab_helper.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr char kTestEmail[] = "email@gmail.com";
constexpr signin_metrics::AccessPoint kTestAccessPoint =
    signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt;

constexpr char kConfirmationNoUnsyncedHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.NoUnsynced";
constexpr char kConfirmationUnsyncedHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.Unsynced";
constexpr char kConfirmationUnsyncedReauthHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.UnsyncedReauth";
constexpr char kConfirmationSupervisedProfileHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.SupervisedProfile";
constexpr char16_t kTestExtensionName[] = u"Test extension";

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
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      histogram_name = kConfirmationSupervisedProfileHistogramName;
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

  SignoutConfirmationUI* TriggerSignoutAndWaitForConfirmationPrompt() {
    auto url = GURL(chrome::kChromeUISignoutConfirmationURL);
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    auto* signin_view_controller = browser()->signin_view_controller();
    signin_view_controller->SignoutOrReauthWithPrompt(
        kTestAccessPoint,
        signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
        signin_metrics::SourceForRefreshTokenOperation::
            kUserMenu_SignOutAllAccounts);

    observer.Wait();

    CHECK(signin_view_controller->ShowsModalDialog());
    return SignoutConfirmationUI::GetForTesting(
        signin_view_controller->GetModalDialogWebContentsForTesting());
  }

  bool IsSigninTab(
      content::WebContents* tab,
      signin_metrics::AccessPoint access_point = kTestAccessPoint) const {
    DiceTabHelper* dice_tab_helper = DiceTabHelper::FromWebContents(tab);
    if (!dice_tab_helper) {
      return false;
    }

    if (!dice_tab_helper->IsChromeSigninPage()) {
      ADD_FAILURE();
      return false;
    }
    if (dice_tab_helper->signin_access_point() != access_point) {
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
    SigninBrowserTestBaseT::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }
};

class SigninViewControllerBrowserTest
    : public SigninViewControllerBrowserTestBase {
 public:
  SigninViewControllerBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kImprovedSigninUIOnDesktop,
         features::kManagedProfileRequiredInterstitial},
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
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Verify it's you".
  base::HistogramTester histogram_tester;
  signout_confirmation_ui->AcceptDialogForTesting();
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
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Cancel".
  base::HistogramTester histogram_tester;
  signout_confirmation_ui->CancelDialogForTesting();
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
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  base::HistogramTester histogram_tester;
  signout_confirmation_ui->AcceptDialogForTesting();
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
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  base::HistogramTester histogram_tester;
  signout_confirmation_ui->AcceptDialogForTesting();
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
                       SignoutOrReauthWithPrompt_SignOutSupervisedUser) {
  // Setup a primary account for a supervised user.
  AccountInfo primary_account_info = SetPrimaryAccount();
  AccountCapabilitiesTestMutator mutator(&primary_account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Trigger the Chrome signout action.
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  base::HistogramTester histogram_tester;
  signout_confirmation_ui->AcceptDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls,
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

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       UpdateAccessPointOfSignInTab) {
  // Request a sign in tab, which will open a new tab.
  browser()->signin_view_controller()->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kPasswordBubble, std::string());
  EXPECT_TRUE(IsSigninTab(browser()->tab_strip_model()->GetActiveWebContents(),
                          signin_metrics::AccessPoint::kPasswordBubble));

  // Request a sign in tab with a different access point, which will update the
  // existing sign in tab's access point.
  browser()->signin_view_controller()->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kAddressBubble, std::string());
  EXPECT_TRUE(IsSigninTab(browser()->tab_strip_model()->GetActiveWebContents(),
                          signin_metrics::AccessPoint::kAddressBubble));

  EXPECT_TRUE(signin_ui_util::GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kAddressBubble));
  EXPECT_FALSE(signin_ui_util::GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kPasswordBubble));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowModalManagedUserNoticeDialog) {
  AccountInfo account_info;
  account_info.email = "email@example.com";
  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  browser()->signin_view_controller()->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info,
          /*is_oidc_account=*/false,
          /*profile_creation_required_by_policy=*/false,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));
  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->profile()));
  browser()->signin_view_controller()->CloseModalSignin();
  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->profile()));

  browser()->signin_view_controller()->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info,
          /*is_oidc_account=*/false,
          /*profile_creation_required_by_policy=*/true,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));
  EXPECT_TRUE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->profile()));
  browser()->signin_view_controller()->CloseModalSignin();
  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->profile()));
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
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);
  signout_confirmation_ui->AcceptDialogForTesting();

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
