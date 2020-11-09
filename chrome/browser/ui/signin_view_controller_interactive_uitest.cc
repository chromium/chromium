// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/reauth_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

// Synchronously waits for the Sync confirmation to be closed.
class SyncConfirmationClosedObserver : public LoginUIService::Observer {
 public:
  LoginUIService::SyncConfirmationUIClosedResult WaitForConfirmationClosed() {
    run_loop_.Run();
    return *result_;
  }

 private:
  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    run_loop_.Quit();
    result_ = result;
  }

  base::RunLoop run_loop_;
  base::Optional<LoginUIService::SyncConfirmationUIClosedResult> result_;
};

class SigninDialogClosedObserver
    : public SigninViewControllerDelegate::Observer {
 public:
  explicit SigninDialogClosedObserver(SigninViewControllerDelegate* delegate)
      : delegate_(delegate) {
    delegate_->AddObserver(this);
  }

  ~SigninDialogClosedObserver() override {
    if (delegate_) {
      delegate_->RemoveObserver(this);
    }
  }

  void WaitForDialogClosed() { dialog_closed_run_loop_.Run(); }

 private:
  // SigninViewControllerDelegate::Observer:
  void OnModalSigninClosed() override {
    delegate_->RemoveObserver(this);
    delegate_ = nullptr;
    dialog_closed_run_loop_.Quit();
  }

  base::RunLoop dialog_closed_run_loop_;
  SigninViewControllerDelegate* delegate_;
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
  browser()->signin_view_controller()->ShowSignin(
      profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN,
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);

  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());
// Press Ctrl/Cmd+T, which will open a new tab.
#if defined(OS_MAC)
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
                       SyncConfirmationDefaultFocus) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "alice@gmail.com");
  content::TestNavigationObserver content_observer(
      GURL("chrome://sync-confirmation/"));
  content_observer.StartWatchingNewWebContents();
  browser()->signin_view_controller()->ShowModalSyncConfirmationDialog();
  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  content_observer.Wait();

  SyncConfirmationClosedObserver sync_confirmation_observer;
  LoginUIServiceFactory::GetForProfile(browser()->profile())
      ->AddObserver(&sync_confirmation_observer);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));

  LoginUIService::SyncConfirmationUIClosedResult result =
      sync_confirmation_observer.WaitForConfirmationClosed();
  EXPECT_EQ(result, LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
}

// Tests that the confirm button is focused by default in the signin email
// confirmation dialog.
IN_PROC_BROWSER_TEST_F(SignInViewControllerBrowserTest,
                       EmailConfirmationDefaultFocus) {
  content::TestNavigationObserver content_observer(
      GURL("chrome://signin-email-confirmation/"));
  content_observer.StartWatchingNewWebContents();
  base::RunLoop run_loop;
  SigninEmailConfirmationDialog::Action chosen_action;
  browser()->signin_view_controller()->ShowModalSigninEmailConfirmationDialog(
      "alice@gmail.com", "bob@gmail.com",
      base::BindLambdaForTesting(
          [&](SigninEmailConfirmationDialog::Action action) {
            chosen_action = action;
            run_loop.Quit();
          }));
  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  content_observer.Wait();

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  run_loop.Run();
  EXPECT_EQ(chosen_action, SigninEmailConfirmationDialog::CREATE_NEW_USER);
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
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

  SigninDialogClosedObserver dialog_observer(
      browser()->signin_view_controller()->GetModalDialogDelegateForTesting());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  // Default action simply closes the dialog.
  dialog_observer.WaitForDialogClosed();
  EXPECT_FALSE(browser()->signin_view_controller()->ShowsModalDialog());
}
