// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

using base::test::RunUntil;
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
  bool DidSignIn() { return signed_in_; }

  // Blocks and waits until the user signs in. Wait() does not block if a
  // GoogleSigninSucceeded has already occurred.
  void Wait() {
    if (seen_) {
      return;
    }

    base::OneShotTimer timer;
    timer.Start(
        FROM_HERE, base::Seconds(30),
        base::BindOnce(&SignInObserver::OnTimeout, base::Unretained(this)));
    running_ = true;
    message_loop_runner_ = new MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(seen_);
  }

  void OnTimeout() {
    seen_ = false;
    if (!running_) {
      return;
    }
    message_loop_runner_->Quit();
    running_ = false;
    FAIL() << "Sign in observer timed out!";
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kSet) {
      return;
    }

    DVLOG(1) << "Sign in finished: Sync primary account was set.";
    signed_in_ = true;
    QuitLoopRunner();
  }

  void QuitLoopRunner() {
    seen_ = true;
    if (!running_) {
      return;
    }
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
  explicit SyncConfirmationClosedObserver(Browser* browser) {
    login_ui_service_observation_.Observe(
        LoginUIServiceFactory::GetForProfile(browser->profile()));
  }

  void WaitForConfirmationClosed() {
    if (sync_confirmation_closed_) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    sync_confirmation_closed_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool sync_confirmation_closed_ = false;
  base::OnceClosure quit_closure_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      login_ui_service_observation_{this};
};

void RunLoopFor(base::TimeDelta duration) {
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

// Returns the RenderFrameHost where Gaia credentials can be filled in.
content::RenderFrameHost* GetSigninFrame(content::WebContents* web_contents) {
  // Dice displays the Gaia page directly in a tab.
  return web_contents->GetPrimaryMainFrame();
}

// Evaluates a boolean script expression in the signin frame.
bool EvaluateBooleanScriptInSigninFrame(content::WebContents* web_contents,
                                        const std::string& script) {
  return content::EvalJs(GetSigninFrame(web_contents), script).ExtractBool();
}

// Returns whether an element with id |element_id| exists in the signin page.
bool ElementExistsByIdInSigninFrame(content::WebContents* web_contents,
                                    const std::string& element_id) {
  return EvaluateBooleanScriptInSigninFrame(
      web_contents, "document.getElementById('" + element_id + "') != null");
}

// Blocks until an element with an id from |element_ids| exists in the signin
// page.
[[nodiscard]] bool WaitUntilAnyElementExistsInSigninFrame(
    content::WebContents* web_contents,
    const std::vector<std::string>& element_ids) {
  return RunUntil([&web_contents, &element_ids]() -> bool {
    for (const std::string& element_id : element_ids) {
      if (ElementExistsByIdInSigninFrame(web_contents, element_id)) {
        return true;
      }
    }
    return false;
  });
}

enum class SyncConfirmationDialogAction { kConfirm, kCancel, kSettings };

enum class ReauthDialogAction { kConfirm, kCancel };

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::string GetButtonIdForSyncConfirmationDialogAction(
    SyncConfirmationDialogAction action) {
  switch (action) {
    case SyncConfirmationDialogAction::kConfirm:
      return "confirmButton";
    case SyncConfirmationDialogAction::kCancel:
      return "notNowButton";
    case SyncConfirmationDialogAction::kSettings:
      return "settingsButton";
  }
}

std::string GetRadioButtonIdForSigninEmailConfirmationDialogAction(
    SigninEmailConfirmationDialog::Action action) {
  switch (action) {
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
    case SigninEmailConfirmationDialog::CLOSE:
      return "createNewUserRadioButton";
    case SigninEmailConfirmationDialog::START_SYNC:
      return "startSyncRadioButton";
  }
}

std::string GetButtonIdForSigninEmailConfirmationDialogAction(
    SigninEmailConfirmationDialog::Action action) {
  switch (action) {
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
    case SigninEmailConfirmationDialog::START_SYNC:
      return "confirmButton";
    case SigninEmailConfirmationDialog::CLOSE:
      return "closeButton";
  }
}

std::string GetButtonIdForReauthConfirmationDialogAction(
    ReauthDialogAction action) {
  switch (action) {
    case ReauthDialogAction::kConfirm:
      return "confirmButton";
    case ReauthDialogAction::kCancel:
      return "cancelButton";
  }
}

std::string GetButtonSelectorForApp(const std::string& app,
                                    const std::string& button_id) {
  return base::StringPrintf(
      "(document.querySelector('%s') == null ? null :"
      "document.querySelector('%s').shadowRoot.querySelector('#%s'))",
      app.c_str(), app.c_str(), button_id.c_str());
}

bool IsElementReady(content::WebContents* web_contents,
                    const std::string& element_selector) {
  std::string find_element_js = base::StringPrintf(
      "if (document.readyState != 'complete') {"
      "  'DocumentNotReady';"
      "} else if (%s == null) {"
      "  'NotFound';"
      "} else if (%s.hidden) {"
      "  'Hidden';"
      "} else {"
      "  'Ok';"
      "}",
      element_selector.c_str(), element_selector.c_str());
  return content::EvalJs(web_contents, find_element_js).ExtractString() == "Ok";
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace login_ui_test_utils {
class SigninViewControllerTestUtil {
 public:
  static bool TryDismissSyncConfirmationDialog(
      Browser* browser,
      SyncConfirmationDialogAction action) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
    return false;
#else
    SigninViewController* signin_view_controller =
        browser->signin_view_controller();
    DCHECK(signin_view_controller);
    if (!signin_view_controller->ShowsModalDialog()) {
      return false;
    }
    content::WebContents* dialog_web_contents =
        signin_view_controller->GetModalDialogWebContentsForTesting();
    DCHECK(dialog_web_contents);
    std::string button_selector = GetButtonSelectorForApp(
        "sync-confirmation-app",
        GetButtonIdForSyncConfirmationDialogAction(action));
    if (!IsElementReady(dialog_web_contents, button_selector)) {
      return false;
    }

    // This cannot be a synchronous call, because it closes the window as a side
    // effect, which may cause the javascript execution to never finish.
    content::ExecuteScriptAsync(dialog_web_contents,
                                button_selector + ".click();");
    return true;
#endif
  }

  static bool TryCompleteSigninEmailConfirmationDialog(
      Browser* browser,
      SigninEmailConfirmationDialog::Action action) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
    return false;
#else
    SigninViewController* signin_view_controller =
        browser->signin_view_controller();
    DCHECK(signin_view_controller);
    if (!signin_view_controller->ShowsModalDialog()) {
      return false;
    }
    content::WebContents* dialog_web_contents =
        signin_view_controller->GetModalDialogWebContentsForTesting();
    DCHECK(dialog_web_contents);
    std::string radio_button_selector = GetButtonSelectorForApp(
        "signin-email-confirmation-app",
        GetRadioButtonIdForSigninEmailConfirmationDialogAction(action));
    std::string button_selector = GetButtonSelectorForApp(
        "signin-email-confirmation-app",
        GetButtonIdForSigninEmailConfirmationDialogAction(action));
    if (!IsElementReady(dialog_web_contents, button_selector)) {
      return false;
    }

    // This cannot be a synchronous call, because it closes the window as a side
    // effect, which may cause the javascript execution to never finish.
    content::ExecuteScriptAsync(
        dialog_web_contents, base::StringPrintf("%s.click(); %s.click();",
                                                radio_button_selector.c_str(),
                                                button_selector.c_str()));
    return true;
#endif
  }

  static bool TryCompleteReauthConfirmationDialog(Browser* browser,
                                                  ReauthDialogAction action) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
    return false;
#else
    SigninViewController* signin_view_controller =
        browser->signin_view_controller();
    DCHECK(signin_view_controller);
    if (!signin_view_controller->ShowsModalDialog()) {
      return false;
    }

    content::WebContents* dialog_web_contents =
        signin_view_controller->GetModalDialogWebContentsForTesting();
    DCHECK(dialog_web_contents);
    std::string button_selector = GetButtonSelectorForApp(
        "signin-reauth-app",
        GetButtonIdForReauthConfirmationDialogAction(action));
    if (!IsElementReady(dialog_web_contents, button_selector)) {
      return false;
    }

    // This cannot be a synchronous call, because it closes the window as a side
    // effect, which may cause the javascript execution to never finish.
    content::ExecuteScriptAsync(dialog_web_contents,
                                button_selector + ".click();");
    return true;
#endif
  }

  static bool TryCompleteProfileCustomizationDialog(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
    return false;
#else
    SigninViewController* signin_view_controller =
        browser->signin_view_controller();
    DCHECK(signin_view_controller);
    if (!signin_view_controller->ShowsModalDialog()) {
      return false;
    }
    content::WebContents* dialog_web_contents =
        signin_view_controller->GetModalDialogWebContentsForTesting();
    DCHECK(dialog_web_contents);
    std::string button_selector =
        GetButtonSelectorForApp("profile-customization-app", "doneButton");
    if (!IsElementReady(dialog_web_contents, button_selector)) {
      return false;
    }

    // content::ExecJs() might return false because this JavaScript execution
    // terminates the renderer as a side effect.
    std::ignore =
        content::ExecJs(dialog_web_contents, button_selector + ".click();");
    return true;
#endif
  }

  static bool ShowsModalDialog(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
    return false;
#else
    return browser->signin_view_controller()->ShowsModalDialog();
#endif
  }
};

void WaitUntilUIReady(Browser* browser) {
  ASSERT_EQ("ready",
            content::EvalJs(
                browser->tab_strip_model()->GetActiveWebContents(),
                "new Promise(resolve => {"
                "  var handler = function() {"
                "    resolve('ready');"
                "  };"
                "  if (!document.querySelector('inline-login-app').loading_)"
                "    handler();"
                "  else"
                "    document.querySelector('inline-login-app').authenticator_"
                "       .addEventListener('ready', handler);"
                "});"));
}

void SigninInNewGaiaFlow(content::WebContents* web_contents,
                         const std::string& email,
                         const std::string& password) {
  ASSERT_TRUE(
      WaitUntilAnyElementExistsInSigninFrame(web_contents, {"identifierId"}));
  std::string js = "document.getElementById('identifierId').value = '" + email +
                   "'; document.getElementById('identifierNext').click();";
  ASSERT_TRUE(content::ExecJs(GetSigninFrame(web_contents), js));

  // Fill the password input field.
  std::string password_script = kGetPasswordFieldFromDiceSigninPage;
  // Wait until the password field exists.
  ASSERT_TRUE(RunUntil([web_contents, &password_script]() -> bool {
    return EvaluateBooleanScriptInSigninFrame(web_contents,
                                              password_script + " != null");
  })) << "Could not find Dice password field";
  js = password_script + ".value = '" + password + "';";
  js += "document.getElementById('passwordNext').click();";
  ASSERT_TRUE(content::ExecJs(GetSigninFrame(web_contents), js));
}

void SigninInOldGaiaFlow(content::WebContents* web_contents,
                         const std::string& email,
                         const std::string& password) {
  ASSERT_TRUE(WaitUntilAnyElementExistsInSigninFrame(web_contents, {"Email"}));
  std::string js = "document.getElementById('Email').value = '" + email + ";" +
                   "document.getElementById('next').click();";
  ASSERT_TRUE(content::ExecJs(GetSigninFrame(web_contents), js));

  ASSERT_TRUE(WaitUntilAnyElementExistsInSigninFrame(web_contents, {"Passwd"}));
  js = "document.getElementById('Passwd').value = '" + password + "';" +
       "document.getElementById('signIn').click();";
  ASSERT_TRUE(content::ExecJs(GetSigninFrame(web_contents), js));
}

void ExecuteJsToSigninInSigninFrame(content::WebContents* web_contents,
                                    const std::string& email,
                                    const std::string& password) {
  ASSERT_TRUE(WaitUntilAnyElementExistsInSigninFrame(
      web_contents, {"identifierNext", "next"}));
  if (ElementExistsByIdInSigninFrame(web_contents, "identifierNext")) {
    SigninInNewGaiaFlow(web_contents, email, password);
  } else {
    SigninInOldGaiaFlow(web_contents, email, password);
  }
}

bool SignInWithUI(Browser* browser,
                  const std::string& username,
                  const std::string& password,
                  signin::ConsentLevel consent_level) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED_IN_MIGRATION();
  return false;
#else
  SignInObserver signin_observer;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_signin_observation(&signin_observer);
  scoped_signin_observation.Observe(
      IdentityManagerFactory::GetForProfile(browser->profile()));

  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN;

  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      browser->signin_view_controller()->ShowDiceAddAccountTab(
          access_point,
          /*email_hint=*/std::string());
      break;
    case signin::ConsentLevel::kSync:
      browser->signin_view_controller()->ShowDiceEnableSyncTab(
          access_point,
          signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
          /*email_hint=*/std::string());
      break;
  }
  content::WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(active_contents);
  content::TestNavigationObserver observer(
      active_contents, 1, content::MessageLoopRunner::QuitMode::DEFERRED);
  observer.Wait();
  DVLOG(1) << "Sign in user: " << username;
  ExecuteJsToSigninInSigninFrame(active_contents, username, password);
  signin_observer.Wait();
  return signin_observer.DidSignIn();
#endif
}

bool TryUntilSuccessWithTimeout(base::RepeatingCallback<bool()> try_callback,
                                base::TimeDelta timeout) {
  const base::Time expire_time = base::Time::Now() + timeout;
  while (base::Time::Now() <= expire_time) {
    if (try_callback.Run()) {
      return true;
    }
    RunLoopFor(base::Seconds(1));
  }
  return false;
}

bool DismissSyncConfirmationDialog(Browser* browser,
                                   base::TimeDelta timeout,
                                   SyncConfirmationDialogAction action) {
  SyncConfirmationClosedObserver confirmation_closed_observer(browser);

  const base::Time expire_time = base::Time::Now() + timeout;
  while (base::Time::Now() <= expire_time) {
    if (SigninViewControllerTestUtil::TryDismissSyncConfirmationDialog(
            browser, action)) {
      confirmation_closed_observer.WaitForConfirmationClosed();
      EXPECT_FALSE(SigninViewControllerTestUtil::ShowsModalDialog(browser));
      return true;
    }
    RunLoopFor(base::Milliseconds(1000));
  }
  return false;
}

bool ConfirmSyncConfirmationDialog(Browser* browser, base::TimeDelta timeout) {
  return DismissSyncConfirmationDialog(browser, timeout,
                                       SyncConfirmationDialogAction::kConfirm);
}

bool GoToSettingsSyncConfirmationDialog(Browser* browser,
                                        base::TimeDelta timeout) {
  return DismissSyncConfirmationDialog(browser, timeout,
                                       SyncConfirmationDialogAction::kSettings);
}

bool CancelSyncConfirmationDialog(Browser* browser, base::TimeDelta timeout) {
  return DismissSyncConfirmationDialog(browser, timeout,
                                       SyncConfirmationDialogAction::kCancel);
}

bool CompleteSigninEmailConfirmationDialog(
    Browser* browser,
    base::TimeDelta timeout,
    SigninEmailConfirmationDialog::Action action) {
  return TryUntilSuccessWithTimeout(
      base::BindRepeating(SigninViewControllerTestUtil::
                              TryCompleteSigninEmailConfirmationDialog,
                          browser, action),
      timeout);
}

bool CompleteReauthConfirmationDialog(Browser* browser,
                                      base::TimeDelta timeout,
                                      ReauthDialogAction action) {
  return TryUntilSuccessWithTimeout(
      base::BindRepeating(
          SigninViewControllerTestUtil::TryCompleteReauthConfirmationDialog,
          browser, action),
      timeout);
}

bool ConfirmReauthConfirmationDialog(Browser* browser,
                                     base::TimeDelta timeout) {
  return CompleteReauthConfirmationDialog(browser, timeout,
                                          ReauthDialogAction::kConfirm);
}

bool CancelReauthConfirmationDialog(Browser* browser, base::TimeDelta timeout) {
  return CompleteReauthConfirmationDialog(browser, timeout,
                                          ReauthDialogAction::kCancel);
}

bool CompleteProfileCustomizationDialog(Browser* browser,
                                        base::TimeDelta timeout) {
  return TryUntilSuccessWithTimeout(
      base::BindRepeating(
          SigninViewControllerTestUtil::TryCompleteProfileCustomizationDialog,
          browser),
      timeout);
}

}  // namespace login_ui_test_utils
