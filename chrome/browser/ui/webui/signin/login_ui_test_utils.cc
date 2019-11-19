// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

using content::MessageLoopRunner;

// anonymous namespace for signin with UI helper functions.
namespace {

// When Desktop Identity Consistency (Dice) is enabled, the password field is
// not easily accessible on the Gaia page. This script can be used to return it.
const char kGetPasswordFieldFromDiceSigninPage[] =
    "(function() {"
    "  var e = document.getElementById('password');"
    "  if (e == null) return null;"
    "  return e.querySelector('input[type=password]');"
    "})()";

// The SignInObserver observes the identity manager and blocks until a signin
// success or failure notification is fired.
class SignInObserver : public signin::IdentityManager::Observer {
 public:
  SignInObserver() : seen_(false), running_(false), signed_in_(false) {}

  // Returns whether a GoogleSigninSucceeded event has happened.
  bool DidSignIn() {
    return signed_in_;
  }

  // Blocks and waits until the user signs in. Wait() does not block if a
  // GoogleSigninSucceeded has already occurred.
  void Wait() {
    if (seen_)
      return;

    running_ = true;
    message_loop_runner_ = new MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(seen_);
  }

  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override {
    DVLOG(1) << "Google signin succeeded.";
    signed_in_ = true;
    QuitLoopRunner();
  }

  void QuitLoopRunner() {
    seen_ = true;
    if (!running_)
      return;
    message_loop_runner_->Quit();
    running_ = false;
  }

 private:
  // Bool to mark an observed event as seen prior to calling Wait(), used to
  // prevent the observer from blocking.
  bool seen_;
  // True is the message loop runner is running.
  bool running_;
  // True if a GoogleSigninSucceeded event has been observed.
  bool signed_in_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

// Synchronously waits for the Sync confirmation to be closed.
class SyncConfirmationClosedObserver : public LoginUIService::Observer {
 public:
  void WaitForConfirmationClosed() {
    if (sync_confirmation_closed_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    sync_confirmation_closed_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  bool sync_confirmation_closed_ = false;
  base::OnceClosure quit_closure_;
};

void RunLoopFor(base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

// Returns the render frame host where Gaia credentials can be filled in.
content::RenderFrameHost* GetSigninFrame(content::WebContents* web_contents) {
  // Dice displays the Gaia page directly in a tab.
  return web_contents->GetMainFrame();
}

// Waits until the condition is met, by polling.
void WaitUntilCondition(const base::RepeatingCallback<bool()>& condition,
                        const std::string& error_message) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    if (condition.Run())
      return;
    RunLoopFor(base::TimeDelta::FromMilliseconds(1000));
  }

  FAIL() << error_message;
}

// Evaluates a boolean script expression in the signin frame.
bool EvaluateBooleanScriptInSigninFrame(Browser* browser,
                                        const std::string& script) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetSigninFrame(web_contents),
      "window.domAutomationController.send(" + script + ");", &result));
  return result;
}

// Returns whether an element with id |element_id| exists in the signin page.
bool ElementExistsByIdInSigninFrame(Browser* browser,
                                    const std::string& element_id) {
  return EvaluateBooleanScriptInSigninFrame(
      browser, "document.getElementById('" + element_id + "') != null");
}

// Blocks until an element with an id from |element_ids| exists in the signin
// page.
void WaitUntilAnyElementExistsInSigninFrame(
    Browser* browser,
    const std::vector<std::string>& element_ids) {
  WaitUntilCondition(
      base::BindLambdaForTesting([&browser, &element_ids]() -> bool {
        for (const std::string& element_id : element_ids) {
          if (ElementExistsByIdInSigninFrame(browser, element_id))
            return true;
        }
        return false;
      }),
      "Could not find elements in the signin frame");
}

}  // namespace

namespace login_ui_test_utils {
class SigninViewControllerTestUtil {
 public:
  static bool TryDismissSyncConfirmationDialog(Browser* browser) {
#if defined(OS_CHROMEOS)
    NOTREACHED();
    return false;
#else
    SigninViewController* signin_view_controller =
        browser->signin_view_controller();
    DCHECK_NE(signin_view_controller, nullptr);
    if (!signin_view_controller->ShowsModalDialog())
      return false;
    content::WebContents* dialog_web_contents =
        signin_view_controller->GetModalDialogWebContentsForTesting();
    DCHECK_NE(dialog_web_contents, nullptr);
    std::string confirm_button_selector =
        "document.querySelector('sync-confirmation-app').shadowRoot."
        "querySelector('#confirmButton')";
    std::string message;
    std::string find_button_js =
        "if (document.readyState != 'complete') {"
        "  window.domAutomationController.send('DocumentNotReady');"
        "} else if (" +
        confirm_button_selector +
        " == null) {"
        "  window.domAutomationController.send('NotFound');"
        "} else {"
        "  window.domAutomationController.send('Ok');"
        "}";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        dialog_web_contents, find_button_js, &message));
    if (message != "Ok")
      return false;

    // This cannot be a synchronous call, because it closes the window as a side
    // effect, which may cause the javascript execution to never finish.
    content::ExecuteScriptAsync(dialog_web_contents,
                                confirm_button_selector + ".click();");
    return true;
#endif
  }
};

void WaitUntilUIReady(Browser* browser) {
  std::string message;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      "if (!inline.login.getAuthExtHost())"
      "  inline.login.initialize();"
      "var handler = function() {"
      "  window.domAutomationController.send('ready');"
      "};"
      "if (inline.login.isAuthReady())"
      "  handler();"
      "else"
      "  inline.login.getAuthExtHost().addEventListener('ready', handler);",
      &message));
  ASSERT_EQ("ready", message);
}

void SigninInNewGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  WaitUntilAnyElementExistsInSigninFrame(browser, {"identifierId"});
  std::string js = "document.getElementById('identifierId').value = '" + email +
                   "'; document.getElementById('identifierNext').click();";
  ASSERT_TRUE(content::ExecuteScript(GetSigninFrame(web_contents), js));

  // Fill the password input field.
  std::string password_script = kGetPasswordFieldFromDiceSigninPage;
  // Wait until the password field exists.
  WaitUntilCondition(
      base::BindLambdaForTesting([&browser, &password_script]() -> bool {
        return EvaluateBooleanScriptInSigninFrame(browser,
                                                  password_script + " != null");
      }),
      "Could not find Dice password field");
  js = password_script + ".value = '" + password + "';";
  js += "document.getElementById('passwordNext').click();";
  ASSERT_TRUE(content::ExecuteScript(GetSigninFrame(web_contents), js));
}

void SigninInOldGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  WaitUntilAnyElementExistsInSigninFrame(browser, {"Email"});
  std::string js = "document.getElementById('Email').value = '" + email + ";" +
                   "document.getElementById('next').click();";
  ASSERT_TRUE(content::ExecuteScript(GetSigninFrame(web_contents), js));

  WaitUntilAnyElementExistsInSigninFrame(browser, {"Passwd"});
  js = "document.getElementById('Passwd').value = '" + password + "';" +
       "document.getElementById('signIn').click();";
  ASSERT_TRUE(content::ExecuteScript(GetSigninFrame(web_contents), js));
}

void ExecuteJsToSigninInSigninFrame(Browser* browser,
                                    const std::string& email,
                                    const std::string& password) {
  WaitUntilAnyElementExistsInSigninFrame(browser, {"identifierNext", "next"});
  if (ElementExistsByIdInSigninFrame(browser, "identifierNext"))
    SigninInNewGaiaFlow(browser, email, password);
  else
    SigninInOldGaiaFlow(browser, email, password);
}

bool SignInWithUI(Browser* browser,
                  const std::string& username,
                  const std::string& password) {
#if defined(OS_CHROMEOS)
  NOTREACHED();
  return false;
#else
  SignInObserver signin_observer;
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      scoped_signin_observer(&signin_observer);
  scoped_signin_observer.Add(
      IdentityManagerFactory::GetForProfile(browser->profile()));

  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_MENU;
  chrome::ShowBrowserSignin(browser, access_point);
  content::WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(active_contents);
  content::TestNavigationObserver observer(
      active_contents, 1, content::MessageLoopRunner::QuitMode::DEFERRED);
  observer.Wait();
  DVLOG(1) << "Sign in user: " << username;
  ExecuteJsToSigninInSigninFrame(browser, username, password);
  signin_observer.Wait();
  return signin_observer.DidSignIn();
#endif
}

bool DismissSyncConfirmationDialog(Browser* browser, base::TimeDelta timeout) {
  SyncConfirmationClosedObserver confirmation_closed_observer;
  ScopedObserver<LoginUIService, LoginUIService::Observer>
      scoped_confirmation_closed_observer(&confirmation_closed_observer);
  scoped_confirmation_closed_observer.Add(
      LoginUIServiceFactory::GetForProfile(browser->profile()));

  const base::Time expire_time = base::Time::Now() + timeout;
  while (base::Time::Now() <= expire_time) {
    if (SigninViewControllerTestUtil::TryDismissSyncConfirmationDialog(
            browser)) {
      confirmation_closed_observer.WaitForConfirmationClosed();
      return true;
    }
    RunLoopFor(base::TimeDelta::FromMilliseconds(1000));
  }
  return false;
}

}  // namespace login_ui_test_utils
