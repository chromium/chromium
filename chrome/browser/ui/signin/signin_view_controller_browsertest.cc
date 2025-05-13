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
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_sync_util.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/extensions/signin_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

constexpr char kAccountExtensionsSignoutChoiceHistogramName[] =
    "Signin.Extensions.AccountExtensionsSignoutChoice";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kChecked);

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

void VerifySignoutPromptHistogram(
    const base::HistogramTester& histogram_tester,
    ChromeSignoutConfirmationPromptVariant variant,
    ChromeSignoutConfirmationChoice choice) {
  const char* confirmaton_prompt_histogram_name =
      kConfirmationUnsyncedHistogramName;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      confirmaton_prompt_histogram_name = kConfirmationNoUnsyncedHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      confirmaton_prompt_histogram_name =
          kConfirmationUnsyncedReauthHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      confirmaton_prompt_histogram_name =
          kConfirmationSupervisedProfileHistogramName;
      break;
  }

  histogram_tester.ExpectUniqueSample(confirmaton_prompt_histogram_name, choice,
                                      1);
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[confirmaton_prompt_histogram_name] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Signin.ChromeSignoutConfirmationPrompt."),
              testing::ContainerEq(expected_counts));
}

void VerifyUnsyncedDataCountHistograms(
    const base::HistogramTester& histogram_tester,
    ChromeSignoutConfirmationPromptVariant variant) {
  // Unsynced data histograms.
  using syncer::UnsyncedDataRecordingEvent;
  // No records for extensions, because the unsynced data is a bookmark:
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.EXTENSION", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnReauthFromPendingState.EXTENSION", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmationFromPendingState."
      "EXTENSION",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmation.EXTENSION", 0);
  // Records for bookmarks:
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.BOOKMARK", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnReauthFromPendingState.BOOKMARK", 0);
  if (variant == ChromeSignoutConfirmationPromptVariant::kUnsyncedData) {
    histogram_tester.ExpectUniqueSample(
        "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmation.BOOKMARK",
        /*sample=*/1, /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmation.BOOKMARK",
        /*expected_count=*/0);
  }
  if (variant ==
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton) {
    histogram_tester.ExpectUniqueSample(
        "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmationFromPendingState."
        "BOOKMARK",
        /*sample=*/1, /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmationFromPendingState."
        "BOOKMARK",
        /*expected_count=*/0);
  }
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
        syncer::DataTypeSet{syncer::DataType::BOOKMARKS});
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
  SigninViewControllerBrowserTest() = default;

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
  base::test::ScopedFeatureList feature_list_{
      features::kManagedProfileRequiredInterstitial};
};

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    SignoutOrReauthWithPromptForPersistentErrorState_Reauth) {
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

  base::HistogramTester histogram_tester;
  // Trigger the Chrome signout action.
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Verify it's you".
  // Note: This is the cancel action.
  signout_confirmation_ui->CancelDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton,
      ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton);

  // The tab was navigated to the signin page.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSigninTab(tab));
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    SignoutOrReauthWithPromptForPersistentErrorState_SignOutWithUnsyncedData) {
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
  base::HistogramTester histogram_tester;
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  // Note: This is the accept action.
  signout_confirmation_ui->AcceptDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton,
      ChromeSignoutConfirmationChoice::kSignout);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton);

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
                       SignoutOrReauthWithPrompt_Cancel) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Add pending sync data.
  AddUnsyncedData();

  // Trigger the Chrome signout action.
  base::HistogramTester histogram_tester;
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Cancel".
  signout_confirmation_ui->CancelDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
      ChromeSignoutConfirmationChoice::kCancelSignout);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kUnsyncedData);

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
  base::HistogramTester histogram_tester;
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  signout_confirmation_ui->AcceptDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
      ChromeSignoutConfirmationChoice::kSignout);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kUnsyncedData);

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
  base::HistogramTester histogram_tester;
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  signout_confirmation_ui->AcceptDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester, ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
      ChromeSignoutConfirmationChoice::kSignout);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData);

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
  base::HistogramTester histogram_tester;
  SignoutConfirmationUI* signout_confirmation_ui =
      TriggerSignoutAndWaitForConfirmationPrompt();
  ASSERT_TRUE(signout_confirmation_ui);

  // Click "Sign Out Anyway".
  signout_confirmation_ui->AcceptDialogForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls,
      ChromeSignoutConfirmationChoice::kSignout);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls);

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
          /*turn_sync_on_signed_profile=*/false,
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
          /*turn_sync_on_signed_profile=*/false,
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

#if BUILDFLAG(ENABLE_EXTENSIONS)

// A browser test with interactive steps used to test the signout confirmation
// dialog.
class SigninViewControllerInteractiveBrowserTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>>,
      public testing::WithParamInterface<bool> {
 public:
  SigninViewControllerInteractiveBrowserTest() {
    base::FilePath test_data_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
      ADD_FAILURE();
      return;
    }
    extension_data_dir_ = test_data_dir.AppendASCII("extensions");
  }

 protected:
  bool uninstall_account_extensions() const { return GetParam(); }

  const base::FilePath& extension_data_dir() const {
    return extension_data_dir_;
  }

  extensions::ExtensionRegistry* extension_registry() {
    return extensions::ExtensionRegistry::Get(GetProfile());
  }

  AccountInfo SetPrimaryAccount() {
    return identity_test_env()->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
  }

  auto WaitForElementExists(const ui::ElementIdentifier& contents_id,
                            const DeepQuery& element) {
    StateChange element_exists;
    element_exists.type =
        WebContentsInteractionTestUtil::StateChange::Type::kExists;
    element_exists.event = kElementExists;
    element_exists.where = element;
    return WaitForStateChange(contents_id, element_exists);
  }

  // Waits for the dialog to be ready to uninstall account extensions.
  auto WaitForUninstallExtensionsChecked(
      const ui::ElementIdentifier& contents_id) {
    StateChange uninstall_extensions_checked;
    uninstall_extensions_checked.type = WebContentsInteractionTestUtil::
        StateChange::Type::kExistsAndConditionTrue;
    uninstall_extensions_checked.event = kChecked;
    uninstall_extensions_checked.test_function =
        "el => { return el.uninstallExtensionsOnSignoutForTesting(); }";
    uninstall_extensions_checked.where = {"signout-confirmation-app"};
    return WaitForStateChange(contents_id, uninstall_extensions_checked);
  }

  // Accept the dialog, which signs the user out. Optionally, check the checkbox
  // at `kCheckbox` which will specify that account extensions should be
  // uninstalled after signing out.
  auto AcceptDialogAndSignout() {
    const DeepQuery kAcceptButton = {"signout-confirmation-app",
                                     "#acceptButton"};
    const DeepQuery kCheckbox = {"signout-confirmation-app",
                                 "extensions-section", "#checkbox"};

    auto steps = Steps(
        ExecuteJsAt(kWebContentsId, kAcceptButton, "(el) => { el.click(); }"),
        // Verify that the dialog closes correctly.
        WaitForHide(
            SigninViewController::kSignoutConfirmationDialogViewElementId),
        CheckResult(
            [&] {
              return browser()->signin_view_controller()->ShowsModalDialog();
            },
            false),
        // Verify that the user has signed out.
        CheckResult(
            [&] {
              return identity_manager()->HasPrimaryAccount(
                  signin::ConsentLevel::kSignin);
            },
            false));

    // Check the checkbox for uninstalling account extensions in the dialog and
    // wait for the proper state to propagate.
    if (uninstall_account_extensions()) {
      auto steps_plus_click_checkbox = Steps(
          ExecuteJsAt(kWebContentsId, kCheckbox, "(el) => { el.click(); }"),
          WaitForUninstallExtensionsChecked(kWebContentsId));
      steps_plus_click_checkbox += std::move(steps);
      return steps_plus_click_checkbox;
    }

    return steps;
  }

  // Checks if the extension with the given `id` is installed.
  auto CheckExtensionInstalled(const extensions::ExtensionId& id,
                               bool installed) {
    return CheckResult(
        [&]() {
          return extension_registry()->GetInstalledExtension(id) != nullptr;
        },
        installed);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableExtensionsExplicitBrowserSignin};

  // chrome/test/data/extensions/
  base::FilePath extension_data_dir_;
};

// Test that the user's installed account extensions are shown in the signout
// confirmation prompt, then test accepting the dialog with two outcomes based
// on the test variant:
// - UninstallAccountExtensions: account extensions are uninstalled after
//   signing out.
// - KeepAccountExtensions: account extensions are kept after signing out.
IN_PROC_BROWSER_TEST_P(SigninViewControllerInteractiveBrowserTest,
                       ShowAccountExtensionsInSignoutPrompt) {
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler;

  auto load_extension = [this](const std::string& extension_path) {
    extensions::ChromeTestExtensionLoader extension_loader(GetProfile());
    extension_loader.set_pack_extension(true);
    return extension_loader.LoadExtension(
        extension_data_dir().AppendASCII(extension_path));
  };

  // Install a local extension; it should not be shown in the list of account
  // extensions in the dialog.
  scoped_refptr<const extensions::Extension> local_extension =
      load_extension("simple_with_file");
  ASSERT_TRUE(local_extension);
  auto local_extension_id = local_extension->id();

  // Setup a primary account.
  extensions::signin_test_util::SimulateExplicitSignIn(
      GetProfile(), identity_test_env(), kTestEmail);

  // Verify that the user has performed an explicit signin.
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  // And that they can sync extensions while in transport mode.
  ASSERT_TRUE(
      extensions::sync_util::IsSyncingExtensionsInTransportMode(GetProfile()));

  // Install two account extensions: both should eventually be shown in the
  // dialog.
  scoped_refptr<const extensions::Extension> first_account_extension =
      load_extension("simple_with_host");
  ASSERT_TRUE(first_account_extension);
  auto first_account_extension_id = first_account_extension->id();

  scoped_refptr<const extensions::Extension> second_account_extension =
      load_extension("simple_with_icon");
  ASSERT_TRUE(second_account_extension);
  auto second_account_extension_id = second_account_extension->id();

  const int expected_num_account_extensions = 2;

  const DeepQuery kExtensionsSectionExpandButton = {
      "signout-confirmation-app", "extensions-section", "#expandButton"};
  const DeepQuery kExtensionsSectionCollapse = {
      "signout-confirmation-app", "extensions-section", "#collapse"};
  const DeepQuery kExtensionsSectionAccountExtensions = {
      "signout-confirmation-app", "extensions-section",
      "#account-extensions-list"};

  const char* get_num_shown_account_extensions = R"((el) => {
    if (!el.opened) { return -1; }
    return el.querySelectorAll('.account-extension').length;
  })";

  base::HistogramTester histogram_tester;

  // Test sequence setup:
  // - User is signed in and is about to sign out via confirmation prompt.
  // - Use has two account extensions installed while signed in.
  RunTestSequence(
      // Show the dialog and verify that it has shown.
      Do([&] {
        browser()->signin_view_controller()->SignoutOrReauthWithPrompt(
            kTestAccessPoint,
            signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
            signin_metrics::SourceForRefreshTokenOperation::
                kUserMenu_SignOutAllAccounts);
      }),
      WaitForShow(
          SigninViewController::kSignoutConfirmationDialogViewElementId),
      Check([&] {
        return browser()->signin_view_controller()->ShowsModalDialog();
      }),
      InstrumentNonTabWebView(
          kWebContentsId,
          SigninViewController::kSignoutConfirmationDialogViewElementId),

      // Within the dialog, verify that the extensions section is visible but
      // the list of account extensions is collapsed.
      WaitForElementExists(kWebContentsId, kExtensionsSectionExpandButton),
      CheckJsResultAt(kWebContentsId, kExtensionsSectionCollapse,
                      "el => el.opened", false),

      // Click the expand button to open the list of account extensions.
      ExecuteJsAt(kWebContentsId, kExtensionsSectionExpandButton,
                  "(el) => { el.click(); }"),
      WaitForElementExists(kWebContentsId, kExtensionsSectionAccountExtensions),

      // There should be `expected_num_account_extensions` shown in the list.
      CheckJsResultAt(kWebContentsId, kExtensionsSectionCollapse,
                      get_num_shown_account_extensions,
                      expected_num_account_extensions),

      // Now accept the dialog and sign out.
      AcceptDialogAndSignout(),

      // The local extension should always still be installed.
      CheckExtensionInstalled(local_extension_id, true),

      // The account extensions should be uninstalled if the user chose to
      // uninstall them from the dialog based on uninstall_account_extensions().
      CheckExtensionInstalled(first_account_extension_id,
                              !uninstall_account_extensions()),
      CheckExtensionInstalled(second_account_extension_id,
                              !uninstall_account_extensions()));

  AccountExtensionsSignoutChoice choice =
      uninstall_account_extensions()
          ? AccountExtensionsSignoutChoice::kSignoutAccountExtensionsUninstalled
          : AccountExtensionsSignoutChoice::kSignoutAccountExtensionsKept;
  histogram_tester.ExpectUniqueSample(
      kAccountExtensionsSignoutChoiceHistogramName, choice, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         SigninViewControllerInteractiveBrowserTest,
                         testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "UninstallAccountExtensions"
                                             : "KeepAccountExtensions";
                         });

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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
