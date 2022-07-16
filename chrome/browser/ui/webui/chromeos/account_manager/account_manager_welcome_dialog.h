// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_WELCOME_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_WELCOME_DIALOG_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos {

class AccountManagerWelcomeDialog : public SystemWebDialogDelegate {
 public:
  AccountManagerWelcomeDialog(const AccountManagerWelcomeDialog&) = delete;
  AccountManagerWelcomeDialog& operator=(const AccountManagerWelcomeDialog&) =
      delete;

  // Displays the Chrome OS Account Manager welcome screen, if it has not been
  // shown "too many times" before. Returns true if the screen was displayed,
  // false otherwise.
  static bool ShowIfRequired();

  // Displays the Chrome OS Account Manager welcome screen. Only to be used for
  // the EDUCoexistence use case.  Returns true if the screen was displayed,
  // false otherwise.
  static bool ShowIfRequiredForEduCoexistence();

 protected:
  AccountManagerWelcomeDialog();
  ~AccountManagerWelcomeDialog() override;

  // ui::SystemWebDialogDelegate overrides.
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldShowCloseButton() const override;

 private:
  static bool ShowIfRequiredInternal();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::AccountManagerWelcomeDialog;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_WELCOME_DIALOG_H_
