// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

class CredentialLeakDialogController;
class CredentialManagerDialogController;
class PasswordCombinedSelectorController;

// A platform-independent interface for the account chooser dialog.
class AccountChooserPrompt {
 public:
  virtual ~AccountChooserPrompt() = default;

  // Shows the account chooser dialog.
  virtual void ShowAccountChooser() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;
};

// A platform-independent interface for the autosignin promo.
class AutoSigninFirstRunPrompt {
 public:
  virtual ~AutoSigninFirstRunPrompt() = default;

  // Shows the dialog.
  virtual void ShowAutoSigninPrompt() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;
};

// A platform-independent interface for the credentials leaked prompt.
class CredentialLeakPrompt {
 public:
  virtual ~CredentialLeakPrompt() = default;

  // Shows the dialog.
  virtual void ShowCredentialLeakPrompt() = 0;

  // Returns the underlying Widget associated with the on-screen prompt. For
  // Testing Only!
  virtual views::Widget* GetWidgetForTesting() = 0;
};

// Factory function for AccountChooserPrompt on desktop platforms.
std::unique_ptr<AccountChooserPrompt> CreateAccountChooserPromptView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents);

// Factory function for PasswordCombinedSelectorView on desktop platforms.
std::unique_ptr<AccountChooserPrompt> CreatePasswordCombinedSelectorPromptView(
    PasswordCombinedSelectorController* controller,
    content::WebContents* web_contents);

// Factory function for AutoSigninFirstRunPrompt on desktop platforms.
std::unique_ptr<AutoSigninFirstRunPrompt> CreateAutoSigninPromptView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents);

// Factory function for CredentialsLeakedPrompt on desktop platforms.
std::unique_ptr<CredentialLeakPrompt> CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_
