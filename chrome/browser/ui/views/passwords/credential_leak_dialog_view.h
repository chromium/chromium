// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

class CredentialLeakDialogController;
class NonAccessibleImageView;

class CredentialLeakDialogView : public views::DialogDelegateView,
                                 public CredentialLeakPrompt {
 public:
  CredentialLeakDialogView(CredentialLeakDialogController* controller,
                           content::WebContents* web_contents);
  ~CredentialLeakDialogView() override;

  // CredentialsLeakedPrompt:
  void ShowCredentialLeakPrompt() override;
  void ControllerGone() override;

 private:
  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  int GetDialogButtons() const override;
  bool ShouldShowCloseButton() const override;
  void OnThemeChanged() override;

  // Sets up the child views.
  void InitWindow();

  // A weak pointer to the controller.
  CredentialLeakDialogController* controller_ = nullptr;
  content::WebContents* const web_contents_ = nullptr;
  NonAccessibleImageView* image_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CredentialLeakDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_H_
