// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/sanitize_dialog.h"

#include "ash/webui/sanitize_ui/sanitize_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {
std::string GetUrlForPage(SanitizeDialog::SanitizePage page) {
  switch (page) {
    case SanitizeDialog::SanitizePage::kDefault:
      return kChromeUISanitizeAppURL;
  }
}
}  // namespace

const int kSanitizeWindowWidth = 680;
const int kSanitizeWindowHeight = 672;

// static
void SanitizeDialog::ShowDialog(SanitizeDialog::SanitizePage page,
                                gfx::NativeWindow parent) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  auto* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::OS_SANITIZE);
  if (browser) {
    browser->window()->Close();
  }
  // Close any existing Sanitize dialog before reopening.
  MaybeCloseExistingDialog();
  SanitizeDialog* dialog = new SanitizeDialog(page);
  dialog->ShowSystemDialog(parent);
}

void SanitizeDialog::MaybeCloseExistingDialog() {
  SystemWebDialogDelegate* existing_dialog =
      SystemWebDialogDelegate::FindInstance(kSanitizeDialogId);
  if (existing_dialog) {
    existing_dialog->Close();
  }
}

SanitizeDialog::SanitizeDialog(SanitizeDialog::SanitizePage page)
    : SystemWebDialogDelegate(GURL(GetUrlForPage(page)),
                              /*title=*/std::u16string()) {}

SanitizeDialog::~SanitizeDialog() = default;

std::string SanitizeDialog::Id() {
  return dialog_id_;
}

void SanitizeDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Size display_size = display.size();
  display_size = gfx::Size(kSanitizeWindowWidth, kSanitizeWindowHeight);
  *size = display_size;
}

}  // namespace ash
