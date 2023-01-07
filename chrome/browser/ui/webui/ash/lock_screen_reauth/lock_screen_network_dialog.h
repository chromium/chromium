// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/base_lock_dialog.h"

class Profile;

namespace ash {

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

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_DIALOG_H_
