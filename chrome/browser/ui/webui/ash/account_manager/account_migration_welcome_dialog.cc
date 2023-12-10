// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_dialog.h"

#include <string>

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {
constexpr int kSigninDialogWidth = 768;
constexpr int kSigninDialogHeight = 640;

}  // namespace

AccountMigrationWelcomeDialog::AccountMigrationWelcomeDialog(
    const GURL gurl,
    const std::string& email)
    : SystemWebDialogDelegate(gurl, std::u16string() /* title */),
      email_(email),
      id_(gurl.spec()) {}

AccountMigrationWelcomeDialog::~AccountMigrationWelcomeDialog() {}

// static
AccountMigrationWelcomeDialog* AccountMigrationWelcomeDialog::Show(
    const std::string& email) {
  auto* dialogInstance = static_cast<AccountMigrationWelcomeDialog*>(
      SystemWebDialogDelegate::FindInstance(
          GURL(chrome::kChromeUIAccountMigrationWelcomeURL).spec()));

  if (dialogInstance) {
    if (email == dialogInstance->GetUserEmail()) {
      dialogInstance->Focus();
      return dialogInstance;
    }
    // If email is different from the current one - close the current dialog.
    views::Widget::GetWidgetForNativeWindow(dialogInstance->dialog_window())
        ->Close();
  }

  // Dialog's lifetime is managed by itself; don't need to delete.
  auto* dialog = new AccountMigrationWelcomeDialog(
      GURL(chrome::kChromeUIAccountMigrationWelcomeURL), email);
  dialog->ShowSystemDialog();
  return dialog;
}

void AccountMigrationWelcomeDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

std::string AccountMigrationWelcomeDialog::GetUserEmail() const {
  return email_;
}

void AccountMigrationWelcomeDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kSigninDialogWidth, kSigninDialogHeight);
}

std::string AccountMigrationWelcomeDialog::GetDialogArgs() const {
  std::string data;
  base::Value::Dict dialog_args;
  dialog_args.Set("email", email_);
  base::JSONWriter::Write(dialog_args, &data);
  return data;
}

bool AccountMigrationWelcomeDialog::ShouldShowDialogTitle() const {
  return false;
}

bool AccountMigrationWelcomeDialog::ShouldShowCloseButton() const {
  return false;
}

std::string AccountMigrationWelcomeDialog::Id() {
  return id_;
}

}  // namespace ash
