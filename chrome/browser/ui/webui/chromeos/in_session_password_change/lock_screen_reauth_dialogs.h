// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A modal system dialog without any frame decorating it.
class BaseLockDialog : public SystemWebDialogDelegate {
 protected:
  BaseLockDialog(GURL url, gfx::Size desired_size);
  BaseLockDialog(BaseLockDialog const&) = delete;
  ~BaseLockDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  ui::ModalType GetDialogModalType() const override;

 private:
  gfx::Size desired_size_;
};

class LockScreenStartReauthDialog : public BaseLockDialog {
 public:
  LockScreenStartReauthDialog();
  LockScreenStartReauthDialog(LockScreenStartReauthDialog const&) = delete;
  ~LockScreenStartReauthDialog() override;

  void Show();
  void Dismiss();
  bool IsRunning();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
