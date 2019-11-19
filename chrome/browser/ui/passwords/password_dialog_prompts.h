// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_PROMPTS_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
class WebContents;
}

class CredentialLeakDialogController;
class CredentialManagerDialogController;

// The default inset from BubbleFrameView.
const int kTitleTopInset = 12;

// The color of the content in the autosign-in first run prompt.
const SkColor kAutoSigninTextColor = SkColorSetRGB(0x64, 0x64, 0x64);

// The hover color of the account chooser.
const SkColor kButtonHoverColor = SkColorSetRGB(0xEA, 0xEA, 0xEA);

// A platform-independent interface for the account chooser dialog.
class AccountChooserPrompt {
 public:
  // Shows the account chooser dialog.
  virtual void ShowAccountChooser() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;
 protected:
  AccountChooserPrompt() = default;
  virtual ~AccountChooserPrompt() = default;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserPrompt);
};

// A platform-independent interface for the autosignin promo.
class AutoSigninFirstRunPrompt {
 public:
  // Shows the dialog.
  virtual void ShowAutoSigninPrompt() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;
 protected:
  AutoSigninFirstRunPrompt() = default;
  virtual ~AutoSigninFirstRunPrompt() = default;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunPrompt);
};

// A platform-independent interface for the credentials leaked prompt.
class CredentialLeakPrompt {
 public:
  // Shows the dialog.
  virtual void ShowCredentialLeakPrompt() = 0;

  // Notifies the UI element that its controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;

 protected:
  CredentialLeakPrompt() = default;
  virtual ~CredentialLeakPrompt() = default;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakPrompt);
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
