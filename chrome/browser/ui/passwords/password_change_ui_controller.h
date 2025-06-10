// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class DialogModel;
}

// Responsible for creating and displaying appropriate views based on the
// current state of the password change flow.
class PasswordChangeUIController {
 public:
  PasswordChangeUIController(PasswordChangeDelegate* password_change_delegate,
                             base::WeakPtr<content::WebContents> web_contents);
  virtual ~PasswordChangeUIController();

  // Updates the `state_` and the UI.
  virtual void UpdateState(PasswordChangeDelegate::State state);

 private:
  // Handles clicking accept button on the currently displayed dialog.
  void OnDialogAccepted();

  void ShowToast(PasswordChangeToast::ToastOptions options);
  void ShowDialog(std::unique_ptr<ui::DialogModel> dialog_model);

  void CloseDialogWidget(views::Widget::ClosedReason reason);

  // Controls password change process. Owns this class.
  const raw_ptr<PasswordChangeDelegate> password_change_delegate_;

  raw_ptr<PasswordChangeToast> toast_view_;
  std::unique_ptr<views::Widget> toast_widget_;

  base::WeakPtr<content::WebContents> web_contents_;

  // Current state of the password change flow.
  PasswordChangeDelegate::State state_ =
      static_cast<PasswordChangeDelegate::State>(-1);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_
