// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_dialogs.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/confirm_password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {
LockScreenStartReauthDialog* g_dialog = nullptr;
}  // namespace

void LockScreenStartReauthDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = this;
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileHelper::GetLockScreenProfileDir(),
      base::BindRepeating(&LockScreenStartReauthDialog::OnProfileCreated,
                          weak_factory_.GetWeakPtr()));
}

void LockScreenStartReauthDialog::OnProfileCreated(
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    profile_ = profile;
    g_dialog->ShowSystemDialogForBrowserContext(
        profile->GetPrimaryOTRProfile());
    // Show network screen if needed.
    if (!network_state_helper_->IsConnected()) {
      ShowLockScreenNetworkDialog();
    }
  } else if (status != Profile::CREATE_STATUS_CREATED) {
    // TODO(mohammedabdon): Create some generic way to show an error on the lock
    // screen.
    LOG(ERROR) << "Failed to load lockscreen profile";
  }
}

void LockScreenStartReauthDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog)
    g_dialog->Close();
}

void LockScreenStartReauthDialog::OnDialogClosed(
    const std::string& json_retval) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  InSessionPasswordSyncManager* password_sync_manager =
      chromeos::InSessionPasswordSyncManagerFactory::GetForProfile(profile);
  password_sync_manager->ResetDialog();
}

bool LockScreenStartReauthDialog::IsRunning() {
  return g_dialog;
}

void LockScreenStartReauthDialog::CloseLockScreenNetworkDialog() {
  if (!lock_screen_network_dialog_)
    return;
  lock_screen_network_dialog_.reset();
}

void LockScreenStartReauthDialog::ShowLockScreenNetworkDialog() {
  if (lock_screen_network_dialog_)
    return;
  DCHECK(profile_);
  lock_screen_network_dialog_ =
      std::make_unique<chromeos::LockScreenNetworkDialog>(base::BindOnce(
          &LockScreenStartReauthDialog::CloseLockScreenNetworkDialog,
          base::Unretained(this)));
  lock_screen_network_dialog_->Show(profile_);
}

LockScreenStartReauthDialog::LockScreenStartReauthDialog()
    : BaseLockDialog(GURL(chrome::kChromeUILockScreenStartReauthURL),
                     kBaseLockDialogSize),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

LockScreenStartReauthDialog::~LockScreenStartReauthDialog() {
  DCHECK_EQ(this, g_dialog);
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
  CloseLockScreenNetworkDialog();
  g_dialog = nullptr;
}

void LockScreenStartReauthDialog::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (network_state_helper_->IsConnected()) {
    if (lock_screen_network_dialog_) {
      lock_screen_network_dialog_->Close();
    }
    return;
  }
  ShowLockScreenNetworkDialog();
}

void LockScreenStartReauthDialog::DefaultNetworkChanged(
    const NetworkState* network) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
