// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"

namespace chromeos {

class LockScreenNetworkDialog : public BaseLockDialog {
 public:
  using NetworkDialogCleanupCallback = base::OnceCallback<void()>;

  explicit LockScreenNetworkDialog(NetworkDialogCleanupCallback callback);
  LockScreenNetworkDialog(LockScreenNetworkDialog const&) = delete;
  LockScreenNetworkDialog& operator=(const LockScreenNetworkDialog&) = delete;
  ~LockScreenNetworkDialog() override;

  void OnDialogClosed(const std::string& json_retval) override;
  void Show(Profile* profile);
  void Dismiss();

 private:
  NetworkDialogCleanupCallback callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_DIALOG_H_
