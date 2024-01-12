// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class AutoSigninFirstRunDialogView : public views::DialogDelegateView,
                                     public AutoSigninFirstRunPrompt {
  METADATA_HEADER(AutoSigninFirstRunDialogView, views::DialogDelegateView)

 public:
  AutoSigninFirstRunDialogView(CredentialManagerDialogController* controller,
                               content::WebContents* web_contents);
  AutoSigninFirstRunDialogView(const AutoSigninFirstRunDialogView&) = delete;
  AutoSigninFirstRunDialogView& operator=(const AutoSigninFirstRunDialogView&) =
      delete;
  ~AutoSigninFirstRunDialogView() override;

  // AutoSigninFirstRunPrompt:
  void ShowAutoSigninPrompt() override;
  void ControllerGone() override;

 private:
  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  // Sets up the child views.
  void InitWindow();

  // A weak pointer to the controller.
  raw_ptr<CredentialManagerDialogController, AcrossTasksDanglingUntriaged>
      controller_;
  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
