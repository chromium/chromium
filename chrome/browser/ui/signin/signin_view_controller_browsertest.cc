// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_view_controller.h"

#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/logout_tab_helper.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/test_signout_confirmation_handler_waiter.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
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
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/reload_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/signin/account_preview_utils.h"
#include "chrome/browser/ui/webui/signin/cross_device_signin_qr_bubble_ui.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "google_apis/gaia/gaia_id.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/extensions/signin_test_util.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using ::testing::IsEmpty;
using ::testing::Not;

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
constexpr char kConfirmationTooManyBookmarksHistogramName[] =
    "Signin.ChromeSignoutConfirmationPrompt.TooManyBookmarks";
constexpr char16_t kTestExtensionName[] = u"Test extension";

constexpr char kAccountExtensionsSignoutChoiceHistogramName[] =
    "Signin.Extensions.AccountExtensionsSignoutChoice";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kChecked);

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
std::unique_ptr<KeyedService> CreateTestAccountPreviewDataService(
    content::BrowserContext*) {
  return std::make_unique<signin::TestAccountPreviewDataService>();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void VerifySignoutPromptHistogram(
    const base::HistogramTester& histogram_tester,
    ChromeSignoutConfirmationPromptVariant variant,
    ChromeSignoutConfirmationChoice choice) {
  const char* confirmation_prompt_histogram_name;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      confirmation_prompt_histogram_name = kConfirmationNoUnsyncedHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      confirmation_prompt_histogram_name = kConfirmationUnsyncedHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      confirmation_prompt_histogram_name =
          kConfirmationUnsyncedReauthHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      confirmation_prompt_histogram_name =
          kConfirmationSupervisedProfileHistogramName;
      break;
    case ChromeSignoutConfirmationPromptVariant::kTooManyBookmarks:
      confirmation_prompt_histogram_name =
          kConfirmationTooManyBookmarksHistogramName;
      break;
  }

  histogram_tester.ExpectUniqueSample(confirmation_prompt_histogram_name,
                                      choice, 1);
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[confirmation_prompt_histogram_name] = 1;
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

    auto* signin_view_controller = SigninViewController::From(browser());
    signin_view_controller->SignoutOrReauthWithPrompt(
        kTestAccessPoint,
        signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
        signin_metrics::SourceForRefreshTokenOperation::
            kUserMenu_SignOutAllAccounts);

    observer.Wait();

    CHECK(signin_view_controller->ShowsModalDialog());
    SignoutConfirmationUI* signout_confirmation_ui =
        SignoutConfirmationUI::GetForTesting(
            signin_view_controller->GetModalDialogWebContentsForTesting());
    // TODO(crbug.com/469344442): Explore using a standard widget observer
    // checking for the widget's visibility, instead of custom ui observer.
    TestSignoutConfirmationHandlerWaiter handler_observer(
        signout_confirmation_ui);
    handler_observer.Wait();

    return signout_confirmation_ui;
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

 protected:
  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin::TestAccountPreviewDataService* GetTestAccountPreviewDataService() {
    return static_cast<signin::TestAccountPreviewDataService*>(
        AccountPreviewDataServiceFactory::GetForProfile(GetProfile()));
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

 private:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBaseT::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    AccountPreviewDataServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestAccountPreviewDataService));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
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
    SigninViewController::From(browser())
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
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.GetAccountId(),
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
  signout_confirmation_ui->CancelDialogAndReauthForTesting();
  VerifySignoutPromptHistogram(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton,
      ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth);
  VerifyUnsyncedDataCountHistograms(
      histogram_tester,
      ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton);

  // The tab was navigated to the signin page.
  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSigninTab(tab));
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    SignoutOrReauthWithPromptForPersistentErrorState_SignOutWithUnsyncedData) {
  // Setup a primary account in error state.
  AccountInfo primary_account_info = SetPrimaryAccount();
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.GetAccountId(),
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
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_Cancel) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();

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
      primary_account_info.GetAccountId(),
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  // The tab was not navigated to the signin page or signout page.
  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_FALSE(IsSigninTab(tab));
  EXPECT_FALSE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_SignOutWithUnsyncedData) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();

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
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_SignOut) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();

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
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksLimitExceededOnSignoutPrompt", false, 1);

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_NoPrompt) {
  // Setup a primary account in auth error.
  AccountInfo primary_account_info = SetPrimaryAccount();

  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Trigger the Chrome signout action.
  SigninViewController::From(browser())->SignoutOrReauthWithPrompt(
      kTestAccessPoint,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_SignOutSupervisedUser) {
  // Setup a primary account for a supervised user.
  AccountInfo primary_account_info = SetPrimaryAccount();
  AccountCapabilitiesTestMutator mutator(&primary_account_info);
  mutator.set_is_subject_to_parental_controls(true);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

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
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_BookmarksLimitExceeded) {
  // Setup a primary account.
  AccountInfo primary_account_info = SetPrimaryAccount();

  // Set Bookmarks Limit Exceeded error.
  GetTestSyncService()->SetBookmarksLimitExceeded(true);

  {
    // Trigger the Chrome signout action.
    base::HistogramTester histogram_tester;
    SignoutConfirmationUI* signout_confirmation_ui =
        TriggerSignoutAndWaitForConfirmationPrompt();
    ASSERT_TRUE(signout_confirmation_ui);

    // Click "Cancel".
    signout_confirmation_ui->CancelDialogForTesting();
    VerifySignoutPromptHistogram(
        histogram_tester,
        ChromeSignoutConfirmationPromptVariant::kTooManyBookmarks,
        ChromeSignoutConfirmationChoice::kCancelSignout);
    VerifyUnsyncedDataCountHistograms(
        histogram_tester,
        ChromeSignoutConfirmationPromptVariant::kTooManyBookmarks);
    histogram_tester.ExpectUniqueSample(
        "Sync.BookmarksLimitExceededOnSignoutPrompt", true, 1);
  }

  // User is still signed in.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was not navigated to the signin page or signout page.
  content::WebContents* active_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_tab);
  EXPECT_FALSE(IsSigninTab(active_tab));
  EXPECT_FALSE(IsSignoutTab(active_tab));

  {
    // Trigger the Chrome signout action again.
    base::HistogramTester histogram_tester;
    SignoutConfirmationUI* signout_confirmation_ui =
        TriggerSignoutAndWaitForConfirmationPrompt();
    ASSERT_TRUE(signout_confirmation_ui);

    // Click "Sign Out Anyway".
    signout_confirmation_ui->AcceptDialogForTesting();
    VerifySignoutPromptHistogram(
        histogram_tester,
        ChromeSignoutConfirmationPromptVariant::kTooManyBookmarks,
        ChromeSignoutConfirmationChoice::kSignout);
    VerifyUnsyncedDataCountHistograms(
        histogram_tester,
        ChromeSignoutConfirmationPromptVariant::kTooManyBookmarks);
    histogram_tester.ExpectUniqueSample(
        "Sync.BookmarksLimitExceededOnSignoutPrompt", true, 1);
  }

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       SignoutOrReauthWithPrompt_ReauthAndBookmarksLimit) {
  // Setup a primary account in error state.
  AccountInfo primary_account_info = SetPrimaryAccount();
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Set Bookmarks Limit Exceeded error.
  GetTestSyncService()->SetBookmarksLimitExceeded(true);

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

  // If we are in a bookmark limit state, we should not have any unsynced data
  // of type BOOKMARK. The reason is that the BOOKMARK data type is disabled,
  // hence bookmark entities are not forwarded to the commit path.
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeNumUnsyncedEntitiesOnSignoutConfirmationFromPendingState."
      "BOOKMARK",
      /*expected_count=*/0);

  // User was signed out.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The tab was navigated to the signout page.
  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(IsSignoutTab(tab));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninDialogForExtensionsPromptReuseOpenTab) {
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 1);
  ASSERT_TRUE(SigninViewController::IsNTPTab(
      browser()->GetTabStripModel()->GetActiveWebContents()));

  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);

  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(SigninViewController::IsNTPTab(tab));

  ASSERT_FALSE(future.IsReady());
  dialog_delegate->AcceptDialog();

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 1);
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    ShowChromeSigninDialogForExtensionsPromptReuseInactiveOpenTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
  ASSERT_FALSE(SigninViewController::IsNTPTab(
      browser()->GetTabStripModel()->GetActiveWebContents()));

  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);

  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(SigninViewController::IsNTPTab(tab));

  ASSERT_FALSE(future.IsReady());
  dialog_delegate->AcceptDialog();

  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninDialogForExtensionsPromptInNewTab) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 1);
  ASSERT_FALSE(SigninViewController::IsNTPTab(
      browser()->GetTabStripModel()->GetActiveWebContents()));

  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_FALSE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(future.GetCallback());
  ASSERT_TRUE(dialog_delegate);
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);

  content::WebContents* tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
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
    ShowChromeSigninDialogForExtensionsPromptKeyjackingProtection) {
  // Make sure a user is signed in to web (otherwise the dialog doesn't
  // trigger).
  identity_test_env()->MakeAccountAvailable(kTestEmail, {.set_cookie = true});
  ASSERT_THAT(identity_manager()->GetAccountsWithRefreshTokens(),
              Not(IsEmpty()));
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  views::DialogDelegate* dialog_delegate =
      TriggerChromeSigninDialogForExtensionsPrompt(
          /*on_complete=*/base::DoNothing());

  ASSERT_NE(dialog_delegate, nullptr);
  EXPECT_FALSE(dialog_delegate->ShouldAllowKeyEventsDuringInputProtection());
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    ShowChromeSigninDialogForExtensionsPromptNotShownPrimaryAccountSet) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::test::TestFuture<void> future;
  SigninViewController::From(browser())
      ->MaybeShowChromeSigninDialogForExtensions(kTestExtensionName,
                                                 future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(
    SigninViewControllerBrowserTest,
    ShowChromeSigninDialogForExtensionsPromptNotShownNoAccounts) {
  base::test::TestFuture<void> future;
  SigninViewController::From(browser())
      ->MaybeShowChromeSigninDialogForExtensions(kTestExtensionName,
                                                 future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       UpdateAccessPointOfSignInTab) {
  // Request a sign in tab, which will open a new tab.
  SigninViewController::From(browser())->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kPasswordBubble, std::string());
  EXPECT_TRUE(IsSigninTab(browser()->GetTabStripModel()->GetActiveWebContents(),
                          signin_metrics::AccessPoint::kPasswordBubble));

  // Request a sign in tab with a different access point, which will update the
  // existing sign in tab's access point.
  SigninViewController::From(browser())->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kAddressBubble, std::string());
  EXPECT_TRUE(IsSigninTab(browser()->GetTabStripModel()->GetActiveWebContents(),
                          signin_metrics::AccessPoint::kAddressBubble));

  EXPECT_TRUE(signin_ui_util::GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kAddressBubble));
  EXPECT_FALSE(signin_ui_util::GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kPasswordBubble));
}

// When signed out, `ShowDiceAddAccountTab()` uses the `kSigninPrimaryAccount`
// reason and marks Chrome sign-in as in progress.
IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowDiceAddAccountTabReasonSignedOut) {
  SigninViewController::From(browser())->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kRecentTabs, std::string());
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(signin_metrics::Reason::kSigninPrimaryAccount,
            tab_helper->signin_reason());
  EXPECT_TRUE(tab_helper->IsChromeSigninInProgress());
}

// When signed out and `email_hint` matches an existing account, it is a reauth.
IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowDiceAddAccountTabReasonSignedOutReauth) {
  identity_test_env()->MakeAccountAvailable(kTestEmail);
  SigninViewController::From(browser())->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kRecentTabs, kTestEmail);
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(signin_metrics::Reason::kReauthentication,
            tab_helper->signin_reason());
  EXPECT_FALSE(tab_helper->IsChromeSigninInProgress());
}

// When signed in, `ShowDiceAddAccountTab()` uses the `kAddSecondaryAccount`
// reason and does not mark Chrome sign-in as in progress.
IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowDiceAddAccountTabReasonSignedIn) {
  SetPrimaryAccount();
  SigninViewController::From(browser())->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kRecentTabs, std::string());
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(signin_metrics::Reason::kAddSecondaryAccount,
            tab_helper->signin_reason());
  EXPECT_FALSE(tab_helper->IsChromeSigninInProgress());
}

// When signed in and `email_hint` matches an existing account, it is a reauth.
IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowDiceAddAccountTabReasonSignedInReauth) {
  SetPrimaryAccount();
  SigninViewController::From(browser())->ShowDiceAddAccountTab(
      signin_metrics::AccessPoint::kRecentTabs, kTestEmail);
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(signin_metrics::Reason::kReauthentication,
            tab_helper->signin_reason());
  EXPECT_FALSE(tab_helper->IsChromeSigninInProgress());
}

class SigninViewControllerReauthSkipIdentifierBrowserTest
    : public SigninViewControllerBrowserTest {
 private:
  // `kReauthSkipIdentifierPage` makes the reauth URL depend on the state of the
  // cookie jar (see `GetSigninUrlForDiceSigninTab()`): /AddSession while the
  // cookie jar is not fresh, /AccountChooser?ptc=1 once it is fresh and the
  // account is signed out on the web.
  // The feature was enabled in the crash reports of crbug.com/578356808.
  base::test::ScopedFeatureList feature_list_{
      switches::kReauthSkipIdentifierPage};
};

// Regression test for crbug.com/578356808: an NTP tab is reused for a reauth
// flow while its `DiceTabHelper` still holds the signin URL of a previous,
// aborted reauth flow with a different URL.
IN_PROC_BROWSER_TEST_F(SigninViewControllerReauthSkipIdentifierBrowserTest,
                       ReuseNtpTabWithStaleDiceTabHelperState) {
  AccountInfo account = SetPrimaryAccount();
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  const std::string email(account.GetEmail());

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* tab = tab_strip->GetActiveWebContents();

  // First click on the profile menu reauth button, with a stale cookie jar.
  // The NTP tab is overwritten.
  signin_ui_util::ShowReauthForAccount(
      GetProfile(), email,
      signin_metrics::AccessPoint::kProfileMenuPrimaryButton);
  ASSERT_EQ(1, tab_strip->count());
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(tab);
  ASSERT_TRUE(tab_helper);
  const GURL first_signin_url = tab_helper->signin_url();
  EXPECT_TRUE(tab_helper->IsChromeSigninPage());

  // The pending Gaia navigation is aborted by a reload of the NTP.
  ASSERT_TRUE(tab->GetController().GetPendingEntry());
  content::TestNavigationObserver reload_observer(tab);
  tab->GetController().Reload(content::ReloadType::NORMAL,
                              /*check_for_repost=*/false);
  reload_observer.Wait();
  EXPECT_FALSE(tab->GetController().GetPendingEntry());
  EXPECT_NE(first_signin_url, tab->GetLastCommittedURL());
  EXPECT_FALSE(tab_helper->IsChromeSigninPage());
  // The state of the abandoned flow is not reset.
  EXPECT_EQ(first_signin_url, tab_helper->signin_url());

  // The cookie jar becomes fresh, which changes the reauth URL.
  identity_test_env()->SetCookieAccounts(
      {{email, account.GetGaiaId(), /*signed_out=*/true}});

  // Second click on the reauth button, with a different signin URL. The same
  // NTP tab is reused with the stale `DiceTabHelper`.
  signin_ui_util::ShowReauthForAccount(
      GetProfile(), email,
      signin_metrics::AccessPoint::kProfileMenuPrimaryButton);

  // The new reauth flow starts in the reused tab, replacing the stale state.
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(tab, tab_strip->GetActiveWebContents());
  ASSERT_EQ(tab_helper, DiceTabHelper::FromWebContents(tab));
  const GURL second_signin_url = tab_helper->signin_url();
  EXPECT_NE(first_signin_url, second_signin_url);

  EXPECT_TRUE(tab_helper->IsChromeSigninPage());
  // The tab is navigating to the new signin URL.
  ASSERT_TRUE(tab->GetController().GetPendingEntry());
  EXPECT_EQ(second_signin_url,
            tab->GetController().GetPendingEntry()->GetURL());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

class SigninViewControllerCrossDeviceSigninBrowserTest
    : public SigninViewControllerBrowserTestBase {
 public:
  SigninViewControllerCrossDeviceSigninBrowserTest() {
    feature_list_.InitAndEnableFeature(switches::kCrossDeviceSigninFromDesktop);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ShowCrossDeviceSigninQrBubble) {
  SetPrimaryAccount();

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::test::TestFuture<views::Widget*> widget_future;
  observer.set_shown_callback(widget_future.GetRepeatingCallback());

  base::MockCallback<base::OnceClosure> closing_callback;
  EXPECT_CALL(closing_callback, Run()).Times(1);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);
  // Before showing, there should be no explicit state.
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());

  SigninViewController::From(browser())->ShowCrossDeviceSigninQrBubble(
      GURL("https://www.google.com/chrome/go-mobile"), closing_callback.Get(),
      CrossDeviceSigninPromoEntryPoint::kProfileMenu);

  views::Widget* bubble_widget = widget_future.Get();
  ASSERT_TRUE(bubble_widget);

  views::WidgetDelegate* delegate = bubble_widget->widget_delegate();
  ASSERT_TRUE(delegate);
  EXPECT_TRUE(delegate->ShouldShowCloseButton());
  if (auto* bubble_delegate = delegate->AsBubbleDialogDelegate()) {
    EXPECT_FALSE(bubble_delegate->ShouldCloseOnDeactivate());
  }

  views::WebView* web_view = views::AsViewClass<views::WebView>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kCrossDeviceSigninQrBubbleWebViewElementId,
          views::ElementTrackerViews::GetContextForWidget(bubble_widget)));
  ASSERT_TRUE(web_view);

  content::WebContents* web_contents = web_view->GetWebContents();
  ASSERT_TRUE(web_contents);

  // Note: This observer is attached after the WebContents has started loading,
  // so it won't reliably catch load-time JS errors (like missing imports),
  // but it will successfully catch post-load runtime errors or unhandled
  // exceptions.
  content::WebContentsConsoleObserver console_observer(web_contents);
  if (web_contents->IsLoading()) {
    content::WaitForLoadStop(web_contents);
  }
  for (const auto& message : console_observer.messages()) {
    LOG(INFO) << "Console message: " << message.message;
    EXPECT_NE(message.log_level, blink::mojom::ConsoleMessageLevel::kError)
        << "JS Error on WebUI: " << message.message;
  }

  // After showing, the explicit state should be set.
  EXPECT_TRUE(avatar_button->HasExplicitButtonState());

  // Verify that the WebUI URL loaded successfully.
  EXPECT_EQ(web_contents->GetVisibleURL(),
            CrossDeviceSigninQrBubbleUI::GetURLWithEntryPoint(
                CrossDeviceSigninPromoEntryPoint::kProfileMenu));

  // Verify that right-click context menu is disabled in this bubble.
  content::ContextMenuParams params;
  EXPECT_TRUE(web_contents->GetDelegate()->HandleContextMenu(
      *web_contents->GetPrimaryMainFrame(), params));

  views::test::WidgetDestroyedWaiter waiter(bubble_widget);
  // Simulating a click on the avatar button should close the bubble because of
  // the explicit action.
  avatar_button->ButtonPressed(/*is_source_accelerator=*/false);
  waiter.Wait();

  // After closing, wait until the explicit state is cleared (reverted).
  EXPECT_FALSE(avatar_button->HasExplicitButtonState());
}

// Verifies that showing the cross-device sign-in QR bubble from the
// SendTabToSelf entry point displays the expected title and description.
IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ShowCrossDeviceSigninQrBubble_SendTabToSelfEntryPoint) {
  SetPrimaryAccount();

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::test::TestFuture<views::Widget*> widget_future;
  observer.set_shown_callback(widget_future.GetRepeatingCallback());

  base::MockCallback<base::OnceClosure> closing_callback;
  EXPECT_CALL(closing_callback, Run()).Times(1);

  SigninViewController::From(browser())->ShowCrossDeviceSigninQrBubble(
      GURL("https://www.google.com/chrome/go-mobile"), closing_callback.Get(),
      CrossDeviceSigninPromoEntryPoint::kSendTabToSelf);

  views::Widget* bubble_widget = widget_future.Get();
  ASSERT_TRUE(bubble_widget);

  views::WidgetDelegate* delegate = bubble_widget->widget_delegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(
      delegate->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_TITLE));

  views::WebView* web_view = views::AsViewClass<views::WebView>(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kCrossDeviceSigninQrBubbleWebViewElementId,
          views::ElementTrackerViews::GetContextForWidget(bubble_widget)));
  ASSERT_TRUE(web_view);

  content::WebContents* web_contents = web_view->GetWebContents();
  ASSERT_TRUE(web_contents);
  if (web_contents->IsLoading()) {
    content::WaitForLoadStop(web_contents);
  }

  EXPECT_EQ(content::EvalJs(web_contents, "document.title"),
            l10n_util::GetStringUTF8(IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_TITLE));
  EXPECT_EQ(
      content::EvalJs(
          web_contents,
          "document.querySelector('cross-device-signin-qr-bubble-app')"
          ".shadowRoot.querySelector('#subtitle').textContent.trim()"),
      l10n_util::GetStringUTF8(IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_BODY_QR));

  views::test::WidgetDestroyedWaiter waiter(bubble_widget);
  SigninViewController::From(browser())->CloseBubbleSignin();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ClosesOnSignOut) {
  AccountInfo account_info = SetPrimaryAccount();

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::test::TestFuture<views::Widget*> widget_future;
  observer.set_shown_callback(widget_future.GetRepeatingCallback());

  base::MockCallback<base::OnceClosure> closing_callback;
  SigninViewController::From(browser())->ShowCrossDeviceSigninQrBubble(
      GURL("https://www.google.com/chrome/go-mobile"), closing_callback.Get(),
      CrossDeviceSigninPromoEntryPoint::kProfileMenu);
  views::Widget* bubble_widget = widget_future.Get();

  ASSERT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());

  views::test::WidgetDestroyedWaiter waiter(bubble_widget);
  identity_test_env()->ClearPrimaryAccount();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ClosesOnRefreshTokenRemoved) {
  AccountInfo account_info = SetPrimaryAccount();

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::test::TestFuture<views::Widget*> widget_future;
  observer.set_shown_callback(widget_future.GetRepeatingCallback());

  base::MockCallback<base::OnceClosure> closing_callback;
  SigninViewController::From(browser())->ShowCrossDeviceSigninQrBubble(
      GURL("https://www.google.com/chrome/go-mobile"), closing_callback.Get(),
      CrossDeviceSigninPromoEntryPoint::kProfileMenu);
  views::Widget* bubble_widget = widget_future.Get();

  ASSERT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());

  views::test::WidgetDestroyedWaiter waiter(bubble_widget);
  identity_test_env()->RemoveRefreshTokenForAccount(
      account_info.GetAccountId());
  waiter.Wait();
}
IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ClosesOnRefreshTokenError) {
  AccountInfo account_info = SetPrimaryAccount();

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::test::TestFuture<views::Widget*> widget_future;
  observer.set_shown_callback(widget_future.GetRepeatingCallback());

  base::MockCallback<base::OnceClosure> closing_callback;
  SigninViewController::From(browser())->ShowCrossDeviceSigninQrBubble(
      GURL("https://www.google.com/chrome/go-mobile"), closing_callback.Get(),
      CrossDeviceSigninPromoEntryPoint::kProfileMenu);
  views::Widget* bubble_widget = widget_future.Get();

  ASSERT_TRUE(bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());

  views::test::WidgetDestroyedWaiter waiter(bubble_widget);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowSigninInterceptionBubble) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account,
      /*primary_account=*/AccountInfo());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  base::test::TestFuture<SigninInterceptionResult> future;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      SigninViewController::From(browser())->ShowSigninInterceptionBubble(
          bubble_parameters, future.GetCallback());
  ASSERT_TRUE(handle);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(future.IsReady());

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  handle.reset();
  destroyed_waiter.Wait();
  EXPECT_EQ(future.Get(), SigninInterceptionResult::kIgnored);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowSigninInterceptionBubble_ReplacedBySecondBubble) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account,
      /*primary_account=*/AccountInfo());

  views::NamedWidgetShownWaiter widget_waiter1(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  base::test::TestFuture<SigninInterceptionResult> future1;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle1 =
      SigninViewController::From(browser())->ShowSigninInterceptionBubble(
          bubble_parameters, future1.GetCallback());
  ASSERT_TRUE(handle1);
  views::Widget* widget1 = widget_waiter1.WaitIfNeededAndGet();
  ASSERT_TRUE(widget1);

  // Showing a second bubble while the first is open should close the first
  // bubble and invoke its callback with kIgnored.
  views::test::WidgetDestroyedWaiter destroyed_waiter1(widget1);
  views::NamedWidgetShownWaiter widget_waiter2(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  base::test::TestFuture<SigninInterceptionResult> future2;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle2 =
      SigninViewController::From(browser())->ShowSigninInterceptionBubble(
          bubble_parameters, future2.GetCallback());
  ASSERT_TRUE(handle2);

  destroyed_waiter1.Wait();
  EXPECT_EQ(future1.Get(), SigninInterceptionResult::kIgnored);

  views::Widget* widget2 = widget_waiter2.WaitIfNeededAndGet();
  ASSERT_TRUE(widget2);
  EXPECT_TRUE(widget2->IsVisible());

  // Destroying the stale first handle must not close the second bubble.
  handle1.reset();
  EXPECT_FALSE(widget2->IsClosed());
  EXPECT_FALSE(future2.IsReady());

  // Destroying the second handle closes the second bubble.
  views::test::WidgetDestroyedWaiter destroyed_waiter2(widget2);
  handle2.reset();
  destroyed_waiter2.Wait();
  EXPECT_EQ(future2.Get(), SigninInterceptionResult::kIgnored);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninBubble_Accepted) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  SigninViewController::From(browser())->ShowChromeSigninBubble(
      account, signin_metrics::AccessPoint::kMenu);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  auto* web_view = views::AsViewClass<views::WebView>(
      widget->widget_delegate()->GetInitiallyFocusedView());
  ASSERT_TRUE(web_view);
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  ASSERT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      "document.querySelector('chrome-signin-app').shadowRoot."
                      "querySelector('#accept-button').click();"));
  destroyed_waiter.Wait();

  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account.GetAccountId());
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Started",
                                      signin_metrics::AccessPoint::kMenu, 1);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed",
                                      signin_metrics::AccessPoint::kMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.InterceptResult.ChromeSigninNonIntercept",
      SigninInterceptionResult::kAccepted, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.ChromeSignin", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSigninNonIntercept.ResponseTimeAccepted", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.ResponseTimeAccepted", 0);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromChromeSigninInterceptBubble"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Signin_FromChromeSigninInterceptBubble"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Signin_Signin_FromMenu"));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninBubble_Declined) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  SigninViewController::From(browser())->ShowChromeSigninBubble(
      account, signin_metrics::AccessPoint::kMenu);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  auto* web_view = views::AsViewClass<views::WebView>(
      widget->widget_delegate()->GetInitiallyFocusedView());
  ASSERT_TRUE(web_view);
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  ASSERT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      "document.querySelector('chrome-signin-app').shadowRoot."
                      "querySelector('#cancel-button').click();"));
  destroyed_waiter.Wait();

  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectUniqueSample(
      "Signin.InterceptResult.ChromeSigninNonIntercept",
      SigninInterceptionResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.ChromeSignin", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSigninNonIntercept.ResponseTimeDeclined", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.ResponseTimeDeclined", 0);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Impression_FromChromeSigninInterceptBubble"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Signin_Signin_FromChromeSigninInterceptBubble"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Signin_Signin_FromMenu"));
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninBubble_Dismissed) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  base::HistogramTester histogram_tester;

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  SigninViewController::From(browser())->ShowChromeSigninBubble(
      account, signin_metrics::AccessPoint::kMenu);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  ASSERT_TRUE(avatar_button);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  avatar_button->ButtonPressed(/*is_source_accelerator=*/false);
  destroyed_waiter.Wait();

  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester.ExpectUniqueSample(
      "Signin.InterceptResult.ChromeSigninNonIntercept",
      SigninInterceptionResult::kDismissed, 1);
  histogram_tester.ExpectTotalCount("Signin.InterceptResult.ChromeSignin", 0);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.BubbleDismissReason",
      SigninInterceptionDismissReason::kIdentityPillPressed, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.BubbleDismissReason.ChromeSigninNonIntercept",
      SigninInterceptionDismissReason::kIdentityPillPressed, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.BubbleDismissReason.ChromeSignin", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSigninNonIntercept.ResponseTimeDismissed", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.ResponseTimeDismissed", 0);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninBubble_ConflictWithInterceptionBubble) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account,
      /*primary_account=*/AccountInfo());

  views::NamedWidgetShownWaiter widget_waiter1(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  base::test::TestFuture<SigninInterceptionResult> future1;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle1 =
      SigninViewController::From(browser())->ShowSigninInterceptionBubble(
          bubble_parameters, future1.GetCallback());
  ASSERT_TRUE(handle1);
  views::Widget* widget1 = widget_waiter1.WaitIfNeededAndGet();
  ASSERT_TRUE(widget1);

  // Showing a self-owned Chrome Signin bubble replaces the interception bubble.
  views::test::WidgetDestroyedWaiter destroyed_waiter1(widget1);
  views::NamedWidgetShownWaiter widget_waiter2(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  SigninViewController::From(browser())->ShowChromeSigninBubble(
      account, signin_metrics::AccessPoint::kMenu);

  destroyed_waiter1.Wait();
  EXPECT_EQ(future1.Get(), SigninInterceptionResult::kIgnored);

  views::Widget* widget2 = widget_waiter2.WaitIfNeededAndGet();
  ASSERT_TRUE(widget2);
  EXPECT_TRUE(widget2->IsVisible());

  // Destroying the stale interception handle must not close the Chrome Signin
  // bubble.
  handle1.reset();
  EXPECT_FALSE(widget2->IsClosed());

  // Showing a new interception bubble replaces the Chrome Signin bubble without
  // signing the user in.
  views::test::WidgetDestroyedWaiter destroyed_waiter2(widget2);
  views::NamedWidgetShownWaiter widget_waiter3(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  base::test::TestFuture<SigninInterceptionResult> future3;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle3 =
      SigninViewController::From(browser())->ShowSigninInterceptionBubble(
          bubble_parameters, future3.GetCallback());
  ASSERT_TRUE(handle3);

  destroyed_waiter2.Wait();
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  views::Widget* widget3 = widget_waiter3.WaitIfNeededAndGet();
  ASSERT_TRUE(widget3);
  EXPECT_TRUE(widget3->IsVisible());

  views::test::WidgetDestroyedWaiter destroyed_waiter3(widget3);
  handle3.reset();
  destroyed_waiter3.Wait();
  EXPECT_EQ(future3.Get(), SigninInterceptionResult::kIgnored);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowChromeSigninBubble_WithAccountPreviewPreference) {
  AccountInfo account = identity_test_env()->MakeAccountAvailable(kTestEmail);
  signin::AccountPreviewDataService::AccountPreviewPreference pref{
      .gaia_id = account.GetGaiaId(),
      .preferred_data_types =
          {
              {syncer::PASSWORDS, signin::SyncDataQuartile::kAboveQ3},
          },
      .other_device_info =
          signin::PreferredDeviceInfo{
              .form_factor = sync_pb::
                  SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP,
              .enabled_data_types = {syncer::PASSWORDS},
          },
  };
  GetTestAccountPreviewDataService()->SetPreferredAccountForSignin(pref);

  views::NamedWidgetShownWaiter widget_waiter1(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  SigninViewController::From(browser())->ShowChromeSigninBubble(
      account, signin_metrics::AccessPoint::kMenu);

  views::Widget* widget1 = widget_waiter1.WaitIfNeededAndGet();
  ASSERT_TRUE(widget1);
  EXPECT_TRUE(widget1->IsVisible());

  auto* web_view1 = views::AsViewClass<views::WebView>(
      widget1->widget_delegate()->GetInitiallyFocusedView());
  ASSERT_TRUE(web_view1);
  std::optional<std::string> expected_subtitle =
      signin::GetAccountPreviewPromoSubtitle(pref);
  ASSERT_TRUE(expected_subtitle.has_value());
  EXPECT_EQ(
      content::EvalJs(web_view1->GetWebContents(),
                      "document.querySelector('chrome-signin-app').shadowRoot."
                      "querySelector('#subtitle').textContent.trim();"),
      *expected_subtitle);

  // If the preferred account does not match `account`, the default subtitle is
  // used.
  pref.gaia_id = GaiaId("other_gaia_id");
  GetTestAccountPreviewDataService()->SetPreferredAccountForSignin(pref);

  views::test::WidgetDestroyedWaiter destroyed_waiter1(widget1);
  views::NamedWidgetShownWaiter widget_waiter2(
      views::test::AnyWidgetTestPasskey{},
      "DiceWebSigninInterceptionBubbleView");
  SigninViewController::From(browser())->ShowChromeSigninBubble(
      account, signin_metrics::AccessPoint::kMenu);

  destroyed_waiter1.Wait();
  views::Widget* widget2 = widget_waiter2.WaitIfNeededAndGet();
  ASSERT_TRUE(widget2);
  EXPECT_TRUE(widget2->IsVisible());

  auto* web_view2 = views::AsViewClass<views::WebView>(
      widget2->widget_delegate()->GetInitiallyFocusedView());
  ASSERT_TRUE(web_view2);
  std::string expected_default_subtitle = l10n_util::GetStringUTF8(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_SUBTITLE_WITH_BOOKMARKS);
  EXPECT_EQ(
      content::EvalJs(web_view2->GetWebContents(),
                      "document.querySelector('chrome-signin-app').shadowRoot."
                      "querySelector('#subtitle').textContent.trim();"),
      expected_default_subtitle);
}

// Opening the bubble from the profile menu while it is open in another window
// closes the existing bubble and opens it in the requesting window, without the
// profile ever being reported as having no open bubble.
IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ProfileMenuReplacesBubbleOpenInAnotherWindow) {
  SetPrimaryAccount();
  Profile* profile = browser()->GetProfile();
  BrowserWindowInterface* other_browser = CreateBrowser(profile);

  views::NamedWidgetShownWaiter first_bubble_waiter(
      views::test::AnyWidgetTestPasskey{}, "CrossDeviceSigninQrBubbleViews");
  base::MockCallback<base::OnceClosure> first_closing_callback;
  OpenSigninToPhoneQrCodeBubble(browser(),
                                CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                first_closing_callback.Get());
  views::Widget* first_bubble = first_bubble_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(first_bubble);
  ASSERT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(profile));

  std::vector<bool> observed_states;
  base::CallbackListSubscription subscription =
      RegisterCrossDeviceSigninPromoBubbleStateCallback(
          profile, base::BindLambdaForTesting([&](bool is_open) {
            observed_states.push_back(is_open);
          }));

  views::NamedWidgetShownWaiter second_bubble_waiter(
      views::test::AnyWidgetTestPasskey{}, "CrossDeviceSigninQrBubbleViews");
  views::test::WidgetDestroyedWaiter first_bubble_destroyed(first_bubble);
  base::MockCallback<base::OnceClosure> second_closing_callback;
  EXPECT_CALL(first_closing_callback, Run());
  EXPECT_CALL(second_closing_callback, Run()).Times(0);
  OpenSigninToPhoneQrCodeBubble(other_browser,
                                CrossDeviceSigninPromoEntryPoint::kProfileMenu,
                                second_closing_callback.Get());

  // The first bubble is destroyed asynchronously; its destruction must be
  // ignored since it has been replaced.
  first_bubble_destroyed.Wait();
  testing::Mock::VerifyAndClearExpectations(&first_closing_callback);
  views::Widget* second_bubble = second_bubble_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(second_bubble);
  EXPECT_NE(first_bubble, second_bubble);
  EXPECT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(profile));
  EXPECT_TRUE(observed_states.empty());
  testing::Mock::VerifyAndClearExpectations(&second_closing_callback);

  views::test::WidgetDestroyedWaiter second_bubble_destroyed(second_bubble);
  EXPECT_CALL(second_closing_callback, Run());
  second_bubble->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  second_bubble_destroyed.Wait();

  EXPECT_FALSE(IsCrossDeviceSigninPromoBubbleOpen(profile));
  EXPECT_THAT(observed_states, testing::ElementsAre(false));
}

// Closing the window that hosts the bubble marks the bubble as closed.
IN_PROC_BROWSER_TEST_F(SigninViewControllerCrossDeviceSigninBrowserTest,
                       ClosingBrowserWindowClosesBubble) {
  SetPrimaryAccount();
  Profile* profile = browser()->GetProfile();
  BrowserWindowInterface* other_browser = CreateBrowser(profile);

  views::NamedWidgetShownWaiter bubble_waiter(
      views::test::AnyWidgetTestPasskey{}, "CrossDeviceSigninQrBubbleViews");
  base::MockCallback<base::OnceClosure> closing_callback;
  OpenSigninToPhoneQrCodeBubble(other_browser,
                                CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                closing_callback.Get());
  ASSERT_TRUE(bubble_waiter.WaitIfNeededAndGet());
  ASSERT_TRUE(IsCrossDeviceSigninPromoBubbleOpen(profile));

  EXPECT_CALL(closing_callback, Run());
  CloseBrowserSynchronously(other_browser);

  EXPECT_TRUE(base::test::RunUntil(
      [&] { return !IsCrossDeviceSigninPromoBubbleOpen(profile); }));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

IN_PROC_BROWSER_TEST_F(SigninViewControllerBrowserTest,
                       ShowModalManagedUserNoticeDialog) {
  AccountInfo account_info =
      AccountInfo::Builder(GaiaId("gaia_id"), "email@example.com").Build();
  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;
  SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info,
          /*is_oidc_account=*/false,
          /*user_already_signed_in=*/false,
          /*profile_creation_required_by_policy=*/false,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));
  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->GetProfile()));
  SigninViewController::From(browser())->CloseModalSignin();
  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->GetProfile()));

  SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info,
          /*is_oidc_account=*/false,
          /*user_already_signed_in=*/false,
          /*profile_creation_required_by_policy=*/true,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          mock_process_user_choice_callback.Get(), mock_done_callback.Get()));
  EXPECT_TRUE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->GetProfile()));
  SigninViewController::From(browser())->CloseModalSignin();
  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
      browser()->GetProfile()));
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
  const DeepQuery kExtensionsSectionExpandButton = {
      "signout-confirmation-app", "extensions-section", "#expandButton"};
  const DeepQuery kExtensionsSectionCollapse = {
      "signout-confirmation-app", "extensions-section", "#collapse"};
  const DeepQuery kExtensionsSectionAccountExtensions = {
      "signout-confirmation-app", "extensions-section",
      "#account-extensions-list"};

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

  // Show the signout confirmation dialog and instrument its internal
  // WebContents.
  auto ShowAndInstrumentSignoutConfirmationDialog() {
    return Steps(
        Do([&] {
          SigninViewController::From(browser())->SignoutOrReauthWithPrompt(
              kTestAccessPoint,
              signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
              signin_metrics::SourceForRefreshTokenOperation::
                  kUserMenu_SignOutAllAccounts);
        }),
        WaitForShow(
            SigninViewController::kSignoutConfirmationDialogViewElementId),
        Check([&] {
          return SigninViewController::From(browser())->ShowsModalDialog();
        }),
        InstrumentNonTabWebView(
            kWebContentsId,
            SigninViewController::kSignoutConfirmationDialogViewElementId));
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
              return SigninViewController::From(browser())->ShowsModalDialog();
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

  // Loads the extension from `extension_path`.
  auto LoadExtension(const std::string& extension_path) {
    extensions::ChromeTestExtensionLoader extension_loader(GetProfile());
    extension_loader.set_pack_extension(true);
    return extension_loader.LoadExtension(
        extension_data_dir().AppendASCII(extension_path));
  }

 private:
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
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

  // Install a local extension; it should not be shown in the list of account
  // extensions in the dialog.
  scoped_refptr<const extensions::Extension> local_extension =
      LoadExtension("simple_with_file");
  ASSERT_TRUE(local_extension);
  auto local_extension_id = local_extension->id();

  // Setup a primary account.
  extensions::signin_test_util::SimulateExplicitSignIn(
      GetProfile(), identity_test_env(), kTestEmail);

  // Verify that the user can sync extensions while in transport mode.
  ASSERT_TRUE(
      extensions::sync_util::IsSyncingExtensionsInTransportMode(GetProfile()));

  // Install two account extensions: both should eventually be shown in the
  // dialog.
  scoped_refptr<const extensions::Extension> first_account_extension =
      LoadExtension("simple_with_host");
  ASSERT_TRUE(first_account_extension);
  auto first_account_extension_id = first_account_extension->id();

  scoped_refptr<const extensions::Extension> second_account_extension =
      LoadExtension("simple_with_icon");
  ASSERT_TRUE(second_account_extension);
  auto second_account_extension_id = second_account_extension->id();

  const int expected_num_account_extensions = 2;

  const char* get_num_shown_account_extensions = R"((el) => {
    if (!el.opened) { return -1; }
    return el.querySelectorAll('.account-extension').length;
  })";

  base::HistogramTester histogram_tester;

  // Test sequence setup:
  // - User is signed in and is about to sign out via confirmation prompt.
  // - User has two account extensions installed while signed in.
  RunTestSequence(
      // Show the dialog and verify that it has shown.
      ShowAndInstrumentSignoutConfirmationDialog(),

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

// Test that the signout confirmation dialog will show account extensions if the
// user has disabled extensions syncing and that the user can choose to
// uninstall them on signout.
IN_PROC_BROWSER_TEST_P(SigninViewControllerInteractiveBrowserTest,
                       ShowAccountExtensionsSyncDisabled) {
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler;

  // Setup a primary account.
  extensions::signin_test_util::SimulateExplicitSignIn(
      GetProfile(), identity_test_env(), kTestEmail);

  // Verify that the user can sync extensions while in transport mode.
  ASSERT_TRUE(
      extensions::sync_util::IsSyncingExtensionsInTransportMode(GetProfile()));

  // Install an account extension.
  scoped_refptr<const extensions::Extension> account_extension =
      LoadExtension("simple_with_host");
  ASSERT_TRUE(account_extension);
  auto account_extension_id = account_extension->id();

  // Disable extension syncing for this user.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kExtensions, false);

  // Install another extension; it should not be treated as an account
  // extension.
  scoped_refptr<const extensions::Extension> non_account_extension =
      LoadExtension("simple_with_icon");
  ASSERT_TRUE(non_account_extension);
  auto non_account_extension_id = non_account_extension->id();

  // Test sequence setup:
  // - User is signed in and is about to sign out via confirmation prompt.
  // - User has one account extension and one non-account extension installed
  //   while signed in.
  RunTestSequence(
      // Show the dialog and verify that it has shown.
      ShowAndInstrumentSignoutConfirmationDialog(),

      // Within the dialog, verify that the extensions section is visible
      // despite extensions not currently being synced.
      WaitForElementExists(kWebContentsId, kExtensionsSectionExpandButton),

      // Now accept the dialog and sign out.
      AcceptDialogAndSignout(),

      // The local extension should always still be installed.
      CheckExtensionInstalled(non_account_extension_id, true),

      // The account extension should be uninstalled if the user chose to
      // uninstall them from the dialog based on uninstall_account_extensions().
      CheckExtensionInstalled(account_extension_id,
                              !uninstall_account_extensions()));
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
      browser()->GetTabStripModel()->GetActiveWebContents();
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
