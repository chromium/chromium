// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_DELEGATE_H_

#include <string>

// UI-facing delegate interface for a password change flow, providing display
// metadata and handling user actions from password change dialogs and toasts.
class PasswordChangeUIDelegate {
 public:
  // Internal state of a password change flow. Corresponds to
  // `PasswordChangeFlowState` in enums.xml. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  // LINT.IfChange(State)
  enum class State {
    // Password change is being offered to the user, waiting for them to accept
    // or reject it.
    kOfferingPasswordChange = 0,

    // Waiting for the user to accept privacy notice.
    kWaitingForAgreement = 1,

    // Delegate is waiting for change password form to appear.
    kWaitingForChangePasswordForm = 2,

    // Change password form wasn't found.
    kChangePasswordFormNotFound = 3,

    // Change password form is detected. Generating and filling password fields.
    // Delegate waits for submission confirmation.
    kChangingPassword = 4,

    // Password is successfully updated.
    kPasswordSuccessfullyChanged = 5,

    // Password change failed.
    kPasswordChangeFailed = 6,

    // One time password (OTP) was detected on a page. The flow is stopped, user
    // input is required.
    kOtpDetected = 7,

    // Password change was canceled by the user.
    kCanceled = 8,

    // The initial state before any UI is displayed. Transitions automatically
    // into kOfferingPasswordChange or kWaitingForAgreement after no OTP is
    // present on a main page.
    kNoState = 9,

    // Login form was detected on a page during an ongoing password change flow.
    // The flow is not stopped, but the user action is required.
    kLoginFormDetected = 10,

    // Deprecated: kLoginFormDetectedUserCanContinue = 11,

    kMaxValue = kLoginFormDetected,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordChangeFlowState)

  virtual ~PasswordChangeUIDelegate() = default;

  // Returns current state.
  virtual State GetCurrentState() const = 0;

  // Returns origin string that is displayed in the UI.
  virtual std::u16string GetDisplayOrigin() const = 0;

  // Called when the user accepts the first-run privacy notice dialog.
  virtual void OnPrivacyNoticeAccepted() = 0;

  // Starts performing password change by looking for a change password form in
  // a hidden tab.
  virtual void StartPasswordChangeFlow() = 0;

  // Called when the user declines the initial dialog offering password change.
  // Terminates the password change flow.
  virtual void OnPasswordChangeDeclined() = 0;

  // Cancels any ongoing password change operation and transitions to kCanceled.
  virtual void CancelPasswordChangeFlow() = 0;

  // Called when the user chooses to retry the login check (by clicking
  // 'Retry' on the toast).
  virtual void RetryLoginCheck() = 0;

  // Brings a tab where password change is ongoing to the foreground and
  // terminates the automated flow.
  virtual void OpenPasswordChangeTab() = 0;

  // Displays password change confirmation bubble (or navigates to password
  // details in password settings if the user navigated away) and terminates
  // the flow.
  virtual void OpenPasswordDetails() = 0;

  // Called when a terminal dialog or toast is dismissed by the user.
  virtual void OnUiDismissed() = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_DELEGATE_H_
