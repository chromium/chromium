// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "components/signin/public/base/consent_level.h"

class Browser;

namespace login_ui_test_utils {

inline constexpr base::TimeDelta kSyncConfirmationDialogTimeout =
    base::Seconds(30);

// Blocks until the login UI is available and ready for authorization.
void WaitUntilUIReady(Browser* browser);

// Executes JavaScript code to sign in a user with email and password to the
// auth iframe hosted by gaia_auth extension. This function automatically
// detects the version of GAIA sign in page to use.
void ExecuteJsToSigninInSigninFrame(content::WebContents* web_contents,
                                    const std::string& email,
                                    const std::string& password);

// Executes JS to sign in the user in the new GAIA sign in flow.
void SigninInNewGaiaFlow(content::WebContents* web_contents,
                         const std::string& email,
                         const std::string& password);

// Executes JS to sign in the user in the old GAIA sign in flow.
void SigninInOldGaiaFlow(content::WebContents* web_contents,
                         const std::string& email,
                         const std::string& password);

// A function to sign in a user using Chrome sign-in UI interface.
// This will block until a signin succeeded or failed notification is observed.
bool SignInWithUI(Browser* browser,
                  const std::string& email,
                  const std::string& password,
                  signin::ConsentLevel consent_level);

// Waits for sync confirmation dialog to get displayed, then executes javascript
// to click on confirm button. Returns false if dialog wasn't dismissed before
// |timeout|.
[[nodiscard]] bool ConfirmSyncConfirmationDialog(
    Browser* browser,
    base::TimeDelta timeout = kSyncConfirmationDialogTimeout);

// Waits for the history sync optin dialog to get displayed, then executes
// javascript to click on confirm button.
// When `wait_for_dismiss` is true, the method returns true if dialog is
// dismissed before `timeout`, false otheriwise.
// When `wait_for_dismiss` is false, the method returns true if the history
// sync optin service completes successfully a flow and is reset before `timeout`
// false otherwise.
// Note: The case with `wait_for_dismiss` set to false, works only for impressions
// of the dialog via the corresponding history sync optin service.
[[nodiscard]] bool ConfirmHistorySyncOptinDialog(
    Browser* browser,
    base::TimeDelta timeout = kSyncConfirmationDialogTimeout,
    bool wait_for_dismiss = true);

// Waits for the history sync optin dialog to get displayed, then executes
// javascript to click on reject button.
// When `wait_for_dismiss` is true, the method returns true if dialog is
// dismissed before `timeout`, false otheriwise.
// When `wait_for_dismiss` is false, the method returns true if the history
// sync optin service completes successfully a flow and is reset before `timeout`
// false otherwise.
// Note: The case with `wait_for_dismiss` set to false, works only for impressions
// of the dialog via the corresponding history sync optin service.
[[nodiscard]] bool RejectHistorySyncOptinDialog(
    Browser* browser,
    base::TimeDelta timeout = kSyncConfirmationDialogTimeout,
    bool wait_for_dismiss = true);

// Waits for sync confirmation dialog to get displayed, then executes javascript
// to click on settings button. Returns false if dialog wasn't dismissed before
// |timeout|.
[[nodiscard]] bool GoToSettingsSyncConfirmationDialog(
    Browser* browser,
    base::TimeDelta timeout = kSyncConfirmationDialogTimeout);

// Waits for sync confirmation dialog to get displayed, then executes javascript
// to click on cancel button. Returns false if dialog wasn't dismissed before
// |timeout|.
[[nodiscard]] bool CancelSyncConfirmationDialog(
    Browser* browser,
    base::TimeDelta timeout = kSyncConfirmationDialogTimeout);

// Waits for the signin email confirmation dialog to get displayed, then
// executes javascript to perform |action|. Returns false if failed to dismiss
// the dialog before |timeout|.
bool CompleteSigninEmailConfirmationDialog(
    Browser* browser,
    base::TimeDelta timeout,
    SigninEmailConfirmationDialog::Action action);

// Waits for profile customization dialog to get displayed, then executes
// javascript to click on done button. Returns false if dialog wasn't
// dismissed before |timeout|.
bool CompleteProfileCustomizationDialog(
    Browser* browser,
    base::TimeDelta timeout = kSyncConfirmationDialogTimeout);

// Waits for an element from the sign-in page to appear.
void WaitForSigninPageToLoad(content::WebContents* web_contents);
}  // namespace login_ui_test_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
