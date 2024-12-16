// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace content {
class WebContents;
}

class CredentialLeakDialogController;
class CredentialManagerDialogController;

// A platform-independent interface for the account chooser dialog.
class AccountChooserPrompt {
 public:
  AccountChooserPrompt(const AccountChooserPrompt&) = delete;
  AccountChooserPrompt& operator=(const AccountChooserPrompt&) = delete;

  // Shows the account chooser dialog.
  virtual void ShowAccountChooser() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;

 protected:
  AccountChooserPrompt() = default;
  virtual ~AccountChooserPrompt() = default;
};

// A platform-independent interface for the autosignin promo.
class AutoSigninFirstRunPrompt {
 public:
  AutoSigninFirstRunPrompt(const AutoSigninFirstRunPrompt&) = delete;
  AutoSigninFirstRunPrompt& operator=(const AutoSigninFirstRunPrompt&) = delete;

  // Shows the dialog.
  virtual void ShowAutoSigninPrompt() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;

 protected:
  AutoSigninFirstRunPrompt() = default;
  virtual ~AutoSigninFirstRunPrompt() = default;
};

// A platform-independent interface for the credentials leaked prompt.
class CredentialLeakPrompt {
 public:
  CredentialLeakPrompt(const CredentialLeakPrompt&) = delete;
  CredentialLeakPrompt& operator=(const CredentialLeakPrompt&) = delete;

  // Shows the dialog.
  virtual void ShowCredentialLeakPrompt() = 0;

  // Notifies the UI element that its controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;

 protected:
  CredentialLeakPrompt() = default;
  virtual ~CredentialLeakPrompt() = default;
};

// Factory function for AccountChooserPrompt on desktop platforms.
AccountChooserPrompt* CreateAccountChooserPromptView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents);

// Factory function for AutoSigninFirstRunPrompt on desktop platforms.
AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents);

// Factory function for CredentialsLeakedPrompt on desktop platforms.
CredentialLeakPrompt* CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_
