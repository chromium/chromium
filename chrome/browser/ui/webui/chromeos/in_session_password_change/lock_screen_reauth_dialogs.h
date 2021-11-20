// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class LockScreenNetworkDialog;

class LockScreenStartReauthDialog
    : public BaseLockDialog,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  LockScreenStartReauthDialog();
  LockScreenStartReauthDialog(LockScreenStartReauthDialog const&) = delete;
  ~LockScreenStartReauthDialog() override;

  void Show();
  void Dismiss();
  bool IsRunning();
  int GetDialogWidth();

  void DismissLockScreenNetworkDialog();
  void ShowLockScreenNetworkDialog();
  static gfx::Size CalculateLockScreenReauthDialogSize(
      bool is_new_layout_enabled);

  // Used for waiting for the network dialog in tests.
  // Similar methods exist for the main dialog in InSessionPasswordSyncManager.
  bool IsNetworkDialogLoadedForTesting(base::OnceClosure callback);
  void OnNetworkDialogReadyForTesting();

  LockScreenNetworkDialog* get_network_dialog_for_testing() {
    return lock_screen_network_dialog_.get();
  }

  bool is_network_dialog_visible_for_testing() {
    return is_network_dialog_visible_;
  }

 private:
  void OnProfileCreated(Profile* profile, Profile::CreateStatus status);
  void OnDialogClosed(const std::string& json_retval) override;
  void DeleteLockScreenNetworkDialog();

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

  scoped_refptr<chromeos::NetworkStateInformer> network_state_informer_;
  bool is_network_dialog_visible_ = false;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  std::unique_ptr<LockScreenNetworkDialog> lock_screen_network_dialog_;
  Profile* profile_ = nullptr;

  // A callback that is used to notify tests that the network dialog is loaded.
  base::OnceClosure on_network_dialog_loaded_callback_for_testing_;
  bool is_network_dialog_loaded_for_testing_ = false;

  base::WeakPtrFactory<LockScreenStartReauthDialog> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LockScreenStartReauthDialog;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_DIALOGS_H_
