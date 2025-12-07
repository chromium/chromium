// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
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
#include "components/sync/base/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
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
    browser_->GetFeatures().signin_view_controller()->CloseModalSignin();
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

IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest, Accelerators) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  browser()->GetFeatures().signin_view_controller()->ShowSignin(
      signin_metrics::AccessPoint::kSettings);

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

// Tests that the confirm button is focused by default in the sync confirmation
// dialog.
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest,
                       // TODO(crbug.com/40927355): Re-enable this test
                       DISABLED_SyncConfirmationDefaultFocus) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "alice@gmail.com",
                                      signin::ConsentLevel::kSync);
  content::TestNavigationObserver content_observer(
      GURL("chrome://sync-confirmation/"));
  content_observer.StartWatchingNewWebContents();
  auto* signin_view_controller =
      browser()->GetFeatures().signin_view_controller();
  signin_view_controller->ShowModalSyncConfirmationDialog(
      /*is_signin_intercept=*/false, /*is_sync_promo=*/false);
  EXPECT_TRUE(signin_view_controller->ShowsModalDialog());
  content_observer.Wait();

  SyncConfirmationClosedObserver sync_confirmation_observer(browser());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));

  LoginUIService::SyncConfirmationUIClosedResult result =
      sync_confirmation_observer.WaitForConfirmationClosed();
  EXPECT_EQ(result, LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  EXPECT_FALSE(signin_view_controller->ShowsModalDialog());
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
    const bool result =
        BrowserElements::From(browser())->NotifyEvent(element, event_type);
    CHECK(result);
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
            ->GetFeatures()
            .signin_view_controller()
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
        return browser()
            ->GetFeatures()
            .signin_view_controller()
            ->ShowsModalDialog();
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
          RunSubsequence(
              WaitForEvent(kConstrainedDialogWebViewElementId,
                           kEmailConfirmationCompleted),
              CheckResult([&] { return chosen_action; },
                          SigninEmailConfirmationDialog::CREATE_NEW_USER)),

          // Verify that the dialog closes correctly.
          RunSubsequence(WaitForHide(kConstrainedDialogWebViewElementId),
                         CheckResult(
                             [&] {
                               return browser()
                                   ->GetFeatures()
                                   .signin_view_controller()
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
  auto* signin_view_controller =
      browser()->GetFeatures().signin_view_controller();
  signin_view_controller->ShowModalSigninErrorDialog();
  EXPECT_TRUE(signin_view_controller->ShowsModalDialog());
  content_observer.Wait();

  content::WebContentsDestroyedWatcher dialog_destroyed_watcher(
      signin_view_controller->GetModalDialogWebContentsForTesting());

  // Before sending key events, make sure paint-holding does not drop input
  // events.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(
      signin_view_controller->GetModalDialogWebContentsForTesting());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  // Default action simply closes the dialog.
  dialog_destroyed_watcher.Wait();
  EXPECT_FALSE(signin_view_controller->ShowsModalDialog());
}

enum class DialogButtonEnableState : int {
  kDisabled = 0,
  kEnabled = 1,
};

enum class ButtonToClick : int {
  kAcceptButton = 0,
  kRejectButton = 1,
};

class HistorySyncOptinViewControllerInteractiveBrowserTest
    : public SigninBrowserTestBaseT<InteractiveBrowserTest>,
      public testing::WithParamInterface<ButtonToClick> {
 public:
  const InteractiveBrowserTest::DeepQuery kHistoryOptinAcceptButton = {
      "history-sync-optin-app", "#acceptButton"};
  const InteractiveBrowserTest::DeepQuery kHistoryOptinRejectButton = {
      "history-sync-optin-app", "#rejectButton"};
  const char* kIsDisabledFn = "(e) => { return e.disabled; }";

  auto ClickButton(ui::ElementIdentifier parent_element_id,
                   DeepQuery button_query) {
    return Steps(
        ExecuteJsAt(parent_element_id, button_query, "e => e.click()"));
  }

  auto CheckButtonsState(ui::ElementIdentifier parent_element_id,
                         DialogButtonEnableState state) {
    bool is_disabled = state == DialogButtonEnableState::kDisabled;
    return Steps(CheckJsResultAt(parent_element_id, kHistoryOptinAcceptButton,
                                 kIsDisabledFn, is_disabled),
                 CheckJsResultAt(parent_element_id, kHistoryOptinRejectButton,
                                 kIsDisabledFn, is_disabled));
  }

  const InteractiveBrowserTest::DeepQuery& GetButtonToClick() {
    switch (GetParam()) {
      case ButtonToClick::kAcceptButton:
        return kHistoryOptinAcceptButton;
      case ButtonToClick::kRejectButton:
        return kHistoryOptinRejectButton;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

IN_PROC_BROWSER_TEST_P(HistorySyncOptinViewControllerInteractiveBrowserTest,
                       HistorySyncOptinViewDisablesButtonsAfterClick) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);

  // Sign-in the user.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@gmail.com", signin::ConsentLevel::kSignin);

  int callback_execution_count = 0;
  HistorySyncOptinHelper::FlowCompletedCallback
      history_optin_completed_callback =
          HistorySyncOptinHelper::FlowCompletedCallback(
              base::IgnoreArgs<HistorySyncOptinHelper::ScreenChoiceResult>(
                  base::BindLambdaForTesting([&callback_execution_count]() {
                    callback_execution_count += 1;
                  })));
  bool should_close_modal_dialog = false;

  RunTestSequence(
      // Show the dialog and verify that it has shown.
      Do([&] {
        browser()
            ->GetFeatures()
            .signin_view_controller()
            ->ShowModalHistorySyncOptInDialog(
                should_close_modal_dialog,
                std::move(history_optin_completed_callback));
      }),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      CheckButtonsState(kHistorySyncOptinDialogContentsId,
                        DialogButtonEnableState::kEnabled),
      ClickButton(kHistorySyncOptinDialogContentsId, GetButtonToClick()),
      // The buttons should become disabled.
      CheckButtonsState(kHistorySyncOptinDialogContentsId,
                        DialogButtonEnableState::kDisabled),
      // Regression check for crbug.com/449140137: Clicking again on the button
      // has no effect. In production the button is not even clickable, but here
      // JS manipulation allows us to click again.
      ClickButton(kHistorySyncOptinDialogContentsId, GetButtonToClick()),
      Do([&callback_execution_count]() {
        EXPECT_EQ(1, callback_execution_count);
      }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         HistorySyncOptinViewControllerInteractiveBrowserTest,
                         ::testing::Values(ButtonToClick::kAcceptButton,
                                           ButtonToClick::kRejectButton));
