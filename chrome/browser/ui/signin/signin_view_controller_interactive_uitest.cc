// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/interactive_views_test.h"
#if !BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#endif

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kEmailConfirmationCompleted);

// Synchronously waits for the Sync confirmation to be closed.
class SyncConfirmationClosedObserver : public LoginUIService::Observer {
 public:
  explicit SyncConfirmationClosedObserver(Browser* browser)
      : browser_(browser) {
    DCHECK(browser_);
    login_ui_service_observation_.Observe(
        LoginUIServiceFactory::GetForProfile(browser_->profile()));
  }

  LoginUIService::SyncConfirmationUIClosedResult WaitForConfirmationClosed() {
    run_loop_.Run();
    return *result_;
  }

 private:
  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    login_ui_service_observation_.Reset();
    result_ = result;
    browser_->signin_view_controller()->CloseModalSignin();
    run_loop_.Quit();
  }

  const raw_ptr<Browser> browser_;
  base::RunLoop run_loop_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      login_ui_service_observation_{this};
  std::optional<LoginUIService::SyncConfirmationUIClosedResult> result_;
};

}  // namespace

class SignInViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Many hotkeys are defined by the main menu. The value of these hotkeys
    // depends on the focused window. We must focus the browser window. This is
    // also why this test must be an interactive_ui_test rather than a browser
    // test.
    ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        browser()->window()->GetNativeWindow()));
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }
};

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// DICE sign-in flow isn't applicable on Lacros.
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest, Accelerators) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  browser()->signin_view_controller()->ShowSignin(
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());
// Press Ctrl/Cmd+T, which will open a new tab.
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_T, /*control=*/false, /*shift=*/false, /*alt=*/false,
      /*command=*/true));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_T, /*control=*/true, /*shift=*/false, /*alt=*/false,
      /*command=*/false));
#endif

  wait_for_new_tab.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that the confirm button is focused by default in the sync confirmation
// dialog.
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest,
                       // TODO(crbug.com/40927355): Re-enable this test
                       DISABLED_SyncConfirmationDefaultFocus) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This test runs in the main profile.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  CoreAccountInfo device_primary_account =
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  signin::MakePrimaryAccountAvailable(GetIdentityManager(),
                                      device_primary_account.email,
                                      signin::ConsentLevel::kSync);
#else
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "alice@gmail.com",
                                      signin::ConsentLevel::kSync);
#endif
  content::TestNavigationObserver content_observer(
      GURL("chrome://sync-confirmation/"));
  content_observer.StartWatchingNewWebContents();
  browser()->signin_view_controller()->ShowModalSyncConfirmationDialog(
      /*is_signin_intercept=*/false, /*is_sync_promo=*/false);
  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  content_observer.Wait();

  SyncConfirmationClosedObserver sync_confirmation_observer(browser());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));

  LoginUIService::SyncConfirmationUIClosedResult result =
      sync_confirmation_observer.WaitForConfirmationClosed();
  EXPECT_EQ(result, LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
}

class SignInViewControllerInteractiveBrowserTest
    : public InteractiveBrowserTest {
 public:
  auto WaitForElementExists(const ui::ElementIdentifier& contents_id,
                            const DeepQuery& element) {
    StateChange element_exists;
    element_exists.type =
        WebContentsInteractionTestUtil::StateChange::Type::kExists;
    element_exists.event = kElementExists;
    element_exists.where = element;
    return WaitForStateChange(contents_id, element_exists);
  }

  void SendCustomEvent(ui::ElementIdentifier element,
                       ui::CustomElementEventType event_type) {
    auto* const target =
        ui::ElementTracker::GetElementTracker()->GetUniqueElement(
            element, browser()->window()->GetElementContext());
    ASSERT_NE(nullptr, target);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(target,
                                                                  event_type);
  }
};

// Tests that the confirm button is focused by default in the signin email
// confirmation dialog.
IN_PROC_BROWSER_TEST_F(SignInViewControllerInteractiveBrowserTest,
                       EmailConfirmationDefaultFocus) {
  const DeepQuery kConfirmButton = {"signin-email-confirmation-app",
                                    "#confirmButton"};

  std::optional<SigninEmailConfirmationDialog::Action> chosen_action;

  RunTestSequence(
      // Show the dialog and verify that it has shown.
      Do([&] {
        browser()
            ->signin_view_controller()
            ->ShowModalSigninEmailConfirmationDialog(
                "alice@gmail.com", "bob@gmail.com",
                base::BindLambdaForTesting(
                    [&](SigninEmailConfirmationDialog::Action action) {
                      chosen_action = action;
                      SendCustomEvent(kConstrainedDialogWebViewElementId,
                                      kEmailConfirmationCompleted);
                    }));
      }),
      WaitForShow(kConstrainedDialogWebViewElementId), Check([&] {
        return browser()->signin_view_controller()->ShowsModalDialog();
      }),

      // Confirm the dialog.
      InstrumentNonTabWebView(kWebContentsId,
                              kConstrainedDialogWebViewElementId),
      WaitForElementExists(kWebContentsId, kConfirmButton),
      SendAccelerator(kWebContentsId,
                      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE))
          .SetMustRemainVisible(false),

      // Confirm the results in the UI and model:
      InParallel(

          // Verify that the correct action was selected in confirming the
          // dialog.
          Steps(WaitForEvent(kConstrainedDialogWebViewElementId,
                             kEmailConfirmationCompleted),
                CheckResult([&] { return chosen_action; },
                            SigninEmailConfirmationDialog::CREATE_NEW_USER)),

          // Verify that the dialog closes correctly.
          Steps(WaitForHide(kConstrainedDialogWebViewElementId),
                CheckResult(
                    [&] {
                      return browser()
                          ->signin_view_controller()
                          ->ShowsModalDialog();
                    },
                    false))));
}

// Tests that the confirm button is focused by default in the signin error
// dialog.
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest,
                       ErrorDialogDefaultFocus) {
  content::TestNavigationObserver content_observer(
      GURL("chrome://signin-error/"));
  content_observer.StartWatchingNewWebContents();
  browser()->signin_view_controller()->ShowModalSigninErrorDialog();
  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  content_observer.Wait();

  content::WebContentsDestroyedWatcher dialog_destroyed_watcher(
      browser()
          ->signin_view_controller()
          ->GetModalDialogWebContentsForTesting());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  // Default action simply closes the dialog.
  dialog_destroyed_watcher.Wait();
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
}

// Tests that the confirm button is focused by default in the enterprise
// interception dialog.
// TODO(crbug.com/40943548): Enable the flaky test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_EnterpriseConfirmationDefaultFocus \
  DISABLED_EnterpriseConfirmationDefaultFocus
#else
#define MAYBE_EnterpriseConfirmationDefaultFocus \
  EnterpriseConfirmationDefaultFocus
#endif
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest,
                       MAYBE_EnterpriseConfirmationDefaultFocus) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This test runs in the main profile.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  auto primary_account_info = GetIdentityManager()->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
  auto account_info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), primary_account_info.email,
      signin::ConsentLevel::kSync);
#else
  auto account_info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), "alice@gmail.com", signin::ConsentLevel::kSync);
#endif
  content::TestNavigationObserver content_observer(
      (GURL(chrome::kChromeUIManagedUserProfileNoticeUrl)));
  content_observer.StartWatchingNewWebContents();
  signin::SigninChoice result;
  browser()->signin_view_controller()->ShowModalManagedUserNoticeDialog(
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          account_info, /*is_oidc_account=*/false, /*force_new_profile=*/true,
          /*show_link_data_option=*/true,
          /*process_user_choice_callback=*/
          base::BindOnce([](signin::SigninChoice* result,
                            signin::SigninChoice choice) { *result = choice; },
                         &result),
          /*done_callback=*/
          base::BindOnce(&SigninViewController::CloseModalSignin,
                         browser()->signin_view_controller()->AsWeakPtr())));
  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  content_observer.Wait();

  content::WebContentsDestroyedWatcher dialog_destroyed_watcher(
      browser()
          ->signin_view_controller()
          ->GetModalDialogWebContentsForTesting());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));

  dialog_destroyed_watcher.Wait();
  EXPECT_EQ(result, signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE);
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
}

#if !BUILDFLAG(IS_CHROMEOS)
class SignInViewControllerBrowserOIDCAccountTest
    : public SignInViewControllerBrowserTest {
 protected:
  base::test::ScopedFeatureList features_{
      profile_management::features::kOidcAuthProfileManagement};
};

// Tests that the confirm button is focused by default in the enterprise
// interception dialog.
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserOIDCAccountTest,
                       MAYBE_EnterpriseConfirmationDefaultFocus) {
  auto account_info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), "alice@gmail.com", signin::ConsentLevel::kSync);
  content::TestNavigationObserver content_observer(
      (GURL(chrome::kChromeUIManagedUserProfileNoticeUrl)));
  content_observer.StartWatchingNewWebContents();
  base::RunLoop user_choice_run_loop;
  signin::SigninChoice result;
  DiceWebSigninInterceptorDelegate delegate;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC,
      account_info, account_info);

  auto handle = delegate.ShowOidcInterceptionDialog(
      browser()->tab_strip_model()->GetActiveWebContents(), bubble_parameters,
      base::BindLambdaForTesting(
          [&user_choice_run_loop, &result](
              signin::SigninChoice choice,
              signin::SigninChoiceOperationDoneCallback callback) {
            result = choice;
            std::move(callback).Run(
                signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
            user_choice_run_loop.Quit();
          }),
      /*done_callback=*/
      base::BindOnce(&SigninViewController::CloseModalSignin,
                     browser()->signin_view_controller()->AsWeakPtr()));
  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  content_observer.Wait();

  content::WebContentsDestroyedWatcher dialog_destroyed_watcher(
      browser()
          ->signin_view_controller()
          ->GetModalDialogWebContentsForTesting());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  user_choice_run_loop.Run();
  EXPECT_EQ(result, signin::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE);
  dialog_destroyed_watcher.Wait();
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)
