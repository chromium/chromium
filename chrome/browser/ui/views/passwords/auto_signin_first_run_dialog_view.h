// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

class CredentialManagerDialogController;

class AutoSigninFirstRunDialogView : public views::DialogDelegate,
                                     public AutoSigninFirstRunPrompt {
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
  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  // Sets up the child views.
  void InitWindow();

  // A weak pointer to the controller.
  raw_ptr<CredentialManagerDialogController> controller_;
  const base::WeakPtr<content::WebContents> web_contents_;

  std::unique_ptr<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
