// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_START_REAUTH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_START_REAUTH_UI_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_handler.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// For chrome:://lock-reauth
class LockScreenStartReauthUI : public ui::WebDialogUI {
 public:
  explicit LockScreenStartReauthUI(content::WebUI* web_ui);
  ~LockScreenStartReauthUI() override;

  LockScreenReauthHandler* GetMainHandlerForTests() { return main_handler_; }

 private:
  // The main message handler.
  LockScreenReauthHandler* main_handler_;

  base::WeakPtrFactory<LockScreenStartReauthUI> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LockScreenStartReauthUI;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_START_REAUTH_UI_H_
