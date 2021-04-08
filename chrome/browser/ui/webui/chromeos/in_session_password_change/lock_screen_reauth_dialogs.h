// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class LockScreenNetworkDialog;

class LockScreenStartReauthDialog : public BaseLockDialog,
                                    public NetworkStateHandlerObserver {
 public:
  LockScreenStartReauthDialog();
  LockScreenStartReauthDialog(LockScreenStartReauthDialog const&) = delete;
  ~LockScreenStartReauthDialog() override;

  void Show();
  void Dismiss();
  bool IsRunning();

  void CloseLockScreenNetworkDialog();
  void ShowLockScreenNetworkDialog();

 private:
  void OnProfileCreated(Profile* profile, Profile::CreateStatus status);
  void OnDialogClosed(const std::string& json_retval) override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void DefaultNetworkChanged(const NetworkState* network) override;

  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  std::unique_ptr<LockScreenNetworkDialog> lock_screen_network_dialog_;
  Profile* profile_;

  base::WeakPtrFactory<LockScreenStartReauthDialog> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
