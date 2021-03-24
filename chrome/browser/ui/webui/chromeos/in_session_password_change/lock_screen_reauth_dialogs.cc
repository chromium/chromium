// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_dialogs.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/confirm_password_change_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {

LockScreenStartReauthDialog* g_dialog = nullptr;

constexpr gfx::Size kLockScreenReauthSize(768, 640);

gfx::Size FitSizeToDisplay(const gfx::Size& desired) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  display_size.SetToMin(desired);
  return display_size;
}

}  // namespace

BaseLockDialog::BaseLockDialog(GURL url, gfx::Size desired_size)
    : SystemWebDialogDelegate(url, /*title=*/base::string16()),
      desired_size_(desired_size) {}

BaseLockDialog::~BaseLockDialog() {}

void BaseLockDialog::GetDialogSize(gfx::Size* size) const {
  *size = FitSizeToDisplay(desired_size_);
}

void BaseLockDialog::AdjustWidgetInitParams(views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::ModalType BaseLockDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

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
                          weak_factory_.GetWeakPtr()),
      base::string16(), std::string());
}

void LockScreenStartReauthDialog::OnProfileCreated(
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    g_dialog->ShowSystemDialogForBrowserContext(
        profile->GetPrimaryOTRProfile());
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

LockScreenStartReauthDialog::LockScreenStartReauthDialog()
    : BaseLockDialog(GURL(chrome::kChromeUILockScreenStartReauthURL),
                           kLockScreenReauthSize) {}

LockScreenStartReauthDialog::~LockScreenStartReauthDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

}  // namespace chromeos
