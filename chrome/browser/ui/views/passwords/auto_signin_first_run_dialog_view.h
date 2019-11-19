// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/views/window/dialog_delegate.h"

class AutoSigninFirstRunDialogView : public views::DialogDelegateView,
                                     public AutoSigninFirstRunPrompt {
 public:
  AutoSigninFirstRunDialogView(CredentialManagerDialogController* controller,
                               content::WebContents* web_contents);
  ~AutoSigninFirstRunDialogView() override;

  // AutoSigninFirstRunPrompt:
  void ShowAutoSigninPrompt() override;
  void ControllerGone() override;

 private:
  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;
  void WindowClosing() override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;

  // Sets up the child views.
  void InitWindow();

  // A weak pointer to the controller.
  CredentialManagerDialogController* controller_;
  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
