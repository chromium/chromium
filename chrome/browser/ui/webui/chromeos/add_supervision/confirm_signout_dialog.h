// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_CONFIRM_SIGNOUT_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_CONFIRM_SIGNOUT_DIALOG_H_

#include "base/macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Widget;
}

namespace chromeos {

// Dialog shown when the user tries to close the flow when account has already
// become supervised, and the user only has the choice of finishing the flow, or
// signing out.
class ConfirmSignoutDialog : public views::DialogDelegateView {
 public:
  ~ConfirmSignoutDialog() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate:
  bool Accept() override;
  int GetDialogButtons() const override;

  static void Show();
  static bool IsShowing();

 private:
  ConfirmSignoutDialog();
  static views::Widget* current_instance_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmSignoutDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_CONFIRM_SIGNOUT_DIALOG_H_
