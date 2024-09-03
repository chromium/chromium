// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.h"

#include <string>

#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

std::string GetUrlForPage(DiagnosticsDialog::DiagnosticsPage page) {
  switch (page) {
    case DiagnosticsDialog::DiagnosticsPage::kDefault:
      return kChromeUIDiagnosticsAppUrl;
    case DiagnosticsDialog::DiagnosticsPage::kSystem:
      return base::StrCat({kChromeUIDiagnosticsAppUrl, "?system"});
    case DiagnosticsDialog::DiagnosticsPage::kConnectivity:
      return base::StrCat({kChromeUIDiagnosticsAppUrl, "?connectivity"});
    case DiagnosticsDialog::DiagnosticsPage::kInput:
      return base::StrCat({kChromeUIDiagnosticsAppUrl, "?input"});
  }
}

}  // namespace

// Scale factor for size of the diagnostics dialog, based on display size.
const float kDiagnosticsDialogScale = .8;

// static
void DiagnosticsDialog::ShowDialog(DiagnosticsDialog::DiagnosticsPage page,
                                   gfx::NativeWindow parent) {
  // Close any instance of Diagnostics opened as an SWA.
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::DIAGNOSTICS);
  if (browser) {
    browser->window()->Close();
  }

  // Close any existing Diagnostics dialog before reopening.
  MaybeCloseExistingDialog();

  DiagnosticsDialog* dialog = new DiagnosticsDialog(page);

  // Ensure log controller configuration matches current session.
  diagnostics::DiagnosticsLogController::Get()->ResetAndInitializeLogWriters();

  dialog->ShowSystemDialog(parent);
}

void DiagnosticsDialog::MaybeCloseExistingDialog() {
  SystemWebDialogDelegate* existing_dialog =
      SystemWebDialogDelegate::FindInstance(kDiagnosticsDialogId);
  if (existing_dialog) {
    existing_dialog->Close();
  }
}

DiagnosticsDialog::DiagnosticsDialog(DiagnosticsDialog::DiagnosticsPage page)
    : SystemWebDialogDelegate(GURL(GetUrlForPage(page)),
                              /*title=*/std::u16string()) {}

DiagnosticsDialog::~DiagnosticsDialog() = default;

std::string DiagnosticsDialog::Id() {
  return dialog_id_;
}

void DiagnosticsDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  display_size = gfx::Size(display_size.width() * kDiagnosticsDialogScale,
                           display_size.height() * kDiagnosticsDialogScale);

  *size = display_size;
}

bool DiagnosticsDialog::ShouldCloseDialogOnEscape() const {
  return DiagnosticsDialogUI::ShouldCloseDialogOnEscape();
}

}  // namespace ash
