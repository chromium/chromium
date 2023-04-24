// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/signin/profile_customization_util.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/active_field_trials.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "Unsupported platform"
#endif

namespace {

void FillNonCoreInfo(AccountInfo& account_info, const std::string& given_name) {
  account_info.given_name = given_name;
  account_info.full_name = base::StrCat({given_name, " Doe"});
  account_info.locale = "en";
  account_info.picture_url = base::StrCat({"https://picture.url/", given_name});
  account_info.hosted_domain = kNoHostedDomainFound;
}

}  // namespace

class FirstRunInteractiveUiTest
    : public FirstRunServiceBrowserTestBase,
      public WithProfilePickerInteractiveUiTestHelpers {
 public:
  FirstRunInteractiveUiTest() = default;
  ~FirstRunInteractiveUiTest() override = default;

 protected:
  const std::string kTestGivenName = "Joe";
  const std::string kTestEmail = "joe.consumer@gmail.com";

  // FirstRunServiceBrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    FirstRunServiceBrowserTestBase::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
  }

  content::EvalJsResult EvalJsInPickerContents(const std::string& script) {
    CHECK(view());
    CHECK(view()->GetPickerContents());
    return content::EvalJs(view()->GetPickerContents(), script);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  void SimulateSignInAndWaitForSyncOptInPage(
      const std::string& account_email,
      const std::string& account_given_name) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());
    AccountInfo account_info = signin::MakeAccountAvailableWithCookies(
        identity_manager, test_url_loader_factory(), account_email,
        signin::GetTestGaiaIdForEmail(account_email));
    FillNonCoreInfo(account_info, account_given_name);
    ASSERT_TRUE(account_info.IsValid());

    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
    WaitForLoadStop(AppendSyncConfirmationQueryParams(
        GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow));
  }

 private:
  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
};

IN_PROC_BROWSER_TEST_F(FirstRunInteractiveUiTest, CloseWindow) {
  base::test::TestFuture<bool> proceed_future;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());
  WaitForPickerWidgetCreated();
  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));

  SendCloseWindowKeyboardCommand();
  WaitForPickerClosed();
  EXPECT_EQ(kForYouFreCloseShouldProceed.Get(), proceed_future.Get());

  ASSERT_TRUE(IsProfileNameDefault());

  // Checking the expected metrics from this flow.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(FirstRunInteractiveUiTest,
                       CloseChromeWithKeyboardShortcut) {
  base::test::TestFuture<bool> proceed_future;
  base::HistogramTester histogram_tester;
  base::MockCallback<ProfilePicker::FirstRunExitedCallback>
      first_run_exited_callback;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());
  WaitForPickerWidgetCreated();

  SendQuitAppKeyboardCommand();
  WaitForPickerClosed();

  EXPECT_FALSE(proceed_future.Get());
  histogram_tester.ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kAbandonedFlow, 1);
}
#endif

IN_PROC_BROWSER_TEST_F(FirstRunInteractiveUiTest, SignInAndSync) {
  base::test::TestFuture<bool> proceed_future;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());

  WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  // TODO(crbug.com/1431517): Use a way of activating the button more relevant
  // for an interactive test.
  web_contents()->GetWebUI()->ProcessWebUIMessage(
      web_contents()->GetURL(), "continueWithAccount", base::Value::List());

  WaitForLoadStop(GetSigninChromeSyncDiceUrl());
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  SimulateSignInAndWaitForSyncOptInPage(kTestEmail, kTestGivenName);

  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  LoginUIServiceFactory::GetForProfile(profile())->SyncConfirmationUIClosed(
      LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  WaitForPickerClosed();

  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());

  // Re-assessment of all metrics from this flow, and check for no
  // double-logs.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunInteractiveUiTest, DeclineSync) {
  base::test::TestFuture<bool> proceed_future;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(IsProfileNameDefault());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());

  WaitForPickerWidgetCreated();
  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));

  web_contents()->GetWebUI()->ProcessWebUIMessage(
      web_contents()->GetURL(), "continueWithAccount", base::Value::List());
  WaitForLoadStop(GetSigninChromeSyncDiceUrl());

  SimulateSignInAndWaitForSyncOptInPage(kTestEmail, kTestGivenName);

  LoginUIServiceFactory::GetForProfile(profile())->SyncConfirmationUIClosed(
      LoginUIService::ABORT_SYNC);
  WaitForPickerClosed();

  EXPECT_TRUE(proceed_future.Get());

  EXPECT_EQ(base::ASCIIToUTF16(kTestGivenName), GetProfileName());

  // Checking the expected metrics from this flow.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectTotalCount("Signin.SyncOptIn.Completed", 0);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

// TODO(crbug.com/1433000): Flaky on win-asan
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_PeekAndDeclineSignIn DISABLED_PeekAndDeclineSignIn
#else
#define MAYBE_PeekAndDeclineSignIn PeekAndDeclineSignIn
#endif
IN_PROC_BROWSER_TEST_F(FirstRunInteractiveUiTest, MAYBE_PeekAndDeclineSignIn) {
  const char kAreButtonsDisabledScript[] =
      "(() => {"
      "  const introApp = document.querySelector('intro-app');"
      "  const signInPromo = "
      "      introApp.shadowRoot.querySelector('sign-in-promo');"
      "  return signInPromo.shadowRoot"
      "      .querySelector('#acceptSignInButton').disabled;"
      "})();";

  const char kClickIntroButtonScriptTemplate[] =
      "(() => {"
      "  const introApp = document.querySelector('intro-app');"
      "  const signInPromo = "
      "      introApp.shadowRoot.querySelector('sign-in-promo');"
      "  signInPromo.shadowRoot.querySelector('%s').click();"
      "  return true;"
      "})();";

  const std::string kClickSignInScript = base::StringPrintf(
      kClickIntroButtonScriptTemplate, "#acceptSignInButton");

  const std::string kClickDontSignInScript = base::StringPrintf(
      kClickIntroButtonScriptTemplate, "#declineSignInButton");

  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(IsProfileNameDefault());
  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());

  WaitForPickerWidgetCreated();
  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));

  // Advance to the sign-in page. We actually click the button instead
  // of sending a WebUI message to exercise disabling the buttons.
  EXPECT_EQ(true, EvalJsInPickerContents(kClickSignInScript));
  WaitForLoadStop(GetSigninChromeSyncDiceUrl());

  // The buttons on the intro page should be disabled.
  // We deliberately target the picker's own web contents to run
  // the script, not the currently active web contents from the webview, to
  // get the intro instead of the sign-in page.
  EXPECT_EQ(true, EvalJsInPickerContents(kAreButtonsDisabledScript));

  SendBackKeyboardCommand();

  // There is no event easily observable from the native side indicating that
  // we switched to the previous web contents and that the button status
  // changed. We instead just observe the change from the JS side since Polymer
  // can fire a relevant event.
  const char kEnsureButtonEnabledScript[] =
      "(async () => {"
      "  const introApp = document.querySelector('intro-app');"
      "  const promo = introApp.shadowRoot.querySelector('sign-in-promo');"
      "  const btn = promo.shadowRoot.querySelector('#acceptSignInButton');"
      ""
      "  if (btn.disabled === false) {"
      "    return true;"
      "  }"
      ""
      "  var promiseResolve;"
      "  const promise = new Promise(function(resolve, _){"
      "    promiseResolve = resolve;"
      "  });"
      "  btn._createPropertyObserver('disabled', promiseResolve);"
      "  return await promise.then((newDisabledValue) => {"
      "    return newDisabledValue === false;"
      "  });"
      "})()";
  EXPECT_EQ(true, content::EvalJs(view()->GetPickerContents(),
                                  kEnsureButtonEnabledScript));

  EXPECT_EQ(true, EvalJsInPickerContents(kClickDontSignInScript));
  WaitForPickerClosed();
  EXPECT_EQ(kForYouFreCloseShouldProceed.Get(), proceed_future.Get());

  ASSERT_TRUE(IsProfileNameDefault());

  // Checking the expected metrics from this flow.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}
