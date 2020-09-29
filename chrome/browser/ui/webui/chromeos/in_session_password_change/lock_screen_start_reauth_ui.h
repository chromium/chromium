// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_START_REAUTH_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_START_REAUTH_UI_H_

#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// For chrome:://lock-reauth
class LockScreenStartReauthUI : public ui::WebDialogUI {
 public:
  explicit LockScreenStartReauthUI(content::WebUI* web_ui);
  ~LockScreenStartReauthUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_START_REAUTH_UI_H_
