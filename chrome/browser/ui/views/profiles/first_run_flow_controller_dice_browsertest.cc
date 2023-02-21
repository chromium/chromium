// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"

class FirstRunFlowControllerDiceBrowserTest : public ProfilePickerTestBase {
 public:
  FirstRunFlowControllerDiceBrowserTest() = default;
  ~FirstRunFlowControllerDiceBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ProfilePickerTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&FirstRunFlowControllerDiceBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Clear the previous cookie responses (if any) before using it for a new
    // profile (as test_url_loader_factory() is shared across profiles).
    test_url_loader_factory()->ClearResponses();
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     test_url_loader_factory()));
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::CallbackListSubscription create_services_subscription_;

  base::test::ScopedFeatureList scoped_feature_list_{kForYouFre};
};

IN_PROC_BROWSER_TEST_F(FirstRunFlowControllerDiceBrowserTest, CloseView) {
  base::HistogramTester histogram_tester;
  base::MockCallback<ProfilePicker::FirstRunExitedCallback>
      first_run_exited_callback;
  ProfilePicker::Show(ProfilePicker::Params::ForFirstRun(
      browser()->profile()->GetPath(), first_run_exited_callback.Get()));

  WaitForPickerWidgetCreated();
  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));

  EXPECT_CALL(first_run_exited_callback,
              Run(ProfilePicker::FirstRunExitStatus::kQuitAtEnd));
  ProfilePicker::Hide();
  WaitForPickerClosed();

  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
}

IN_PROC_BROWSER_TEST_F(FirstRunFlowControllerDiceBrowserTest, SignInAndSync) {
  base::HistogramTester histogram_tester;
  base::MockCallback<ProfilePicker::FirstRunExitedCallback>
      first_run_exited_callback;
  Profile* profile = browser()->profile();

  ProfilePicker::Show(ProfilePicker::Params::ForFirstRun(
      profile->GetPath(), first_run_exited_callback.Get()));

  WaitForPickerWidgetCreated();
  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  web_contents()->GetWebUI()->ProcessWebUIMessage(
      web_contents()->GetURL(), "continueWithAccount", base::Value::List());

  WaitForLoadStop(GaiaUrls::GetInstance()->signin_chrome_sync_dice());
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info = signin::MakeAccountAvailableWithCookies(
      identity_manager, test_url_loader_factory(), "joe.consumer@gmail.com",
      signin::GetTestGaiaIdForEmail("joe.consumer@gmail.com"));
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  WaitForLoadStop(AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow));
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);

  base::RunLoop run_loop;
  EXPECT_CALL(first_run_exited_callback,
              Run(ProfilePicker::FirstRunExitStatus::kCompleted))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  LoginUIServiceFactory::GetForProfile(profile)->SyncConfirmationUIClosed(
      LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  WaitForPickerClosed();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Signin.SyncOptIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunFlowControllerDiceBrowserTest,
                       ButtonsAreDisabledOnClickAndEnabledOnNavigateBack) {
  const char kAreButtonsDisabledJSString[] =
      "(() => {"
      "  const introApp = document.querySelector('intro-app');"
      "  const signInPromo = "
      "introApp.shadowRoot.querySelector('sign-in-promo');"
      "  return "
      "signInPromo.shadowRoot.querySelector('#acceptSignInButton').disabled;"
      "})();";

  const char kClickSignInButtonJSString[] =
      "(() => {"
      "  const introApp = document.querySelector('intro-app');"
      "  const signInPromo = "
      "introApp.shadowRoot.querySelector('sign-in-promo');"
      "signInPromo.shadowRoot.querySelector('#acceptSignInButton').click();"
      "return true;"
      "})();";

  base::RunLoop run_loop;
  Profile* profile = browser()->profile();

  ProfilePicker::Show(ProfilePicker::Params::ForFirstRun(
      profile->GetPath(), base::IgnoreArgs<ProfilePicker::FirstRunExitStatus>(
                              run_loop.QuitClosure())));

  WaitForPickerWidgetCreated();
  WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));

  EXPECT_EQ(true, content::EvalJs(view()->GetPickerContents(),
                                  kClickSignInButtonJSString));

  WaitForLoadStop(GaiaUrls::GetInstance()->signin_chrome_sync_dice());
  EXPECT_EQ(true, content::EvalJs(view()->GetPickerContents(),
                                  kAreButtonsDisabledJSString));

  // Navigate back from the sign in step.
  // Use "Command [" for Mac and "Alt Left" for the other operating systems.
#if BUILDFLAG(IS_MAC)
  view()->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN));
#else
  view()->AcceleratorPressed(ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN));
#endif

  EXPECT_EQ(false, content::EvalJs(view()->GetPickerContents(),
                                   kAreButtonsDisabledJSString));

  web_contents()->GetWebUI()->ProcessWebUIMessage(
      web_contents()->GetURL(), "continueWithoutAccount", base::Value::List());
  WaitForPickerClosed();
  run_loop.Run();
}
