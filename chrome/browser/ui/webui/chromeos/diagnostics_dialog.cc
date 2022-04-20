// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/diagnostics_dialog.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/strings/strcat.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {
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
  DiagnosticsDialog* dialog = new DiagnosticsDialog(page);

  // Ensure log controller configuration matches current session.
  if (ash::features::IsLogControllerForDiagnosticsAppEnabled()) {
    ash::diagnostics::DiagnosticsLogController::Get()
        ->ResetAndInitializeLogWriters();
  }

  dialog->ShowSystemDialog(parent);
}

DiagnosticsDialog::DiagnosticsDialog(DiagnosticsDialog::DiagnosticsPage page)
    : SystemWebDialogDelegate(GURL(GetUrlForPage(page)),
                              /*title=*/std::u16string()) {}

DiagnosticsDialog::~DiagnosticsDialog() = default;

const std::string& DiagnosticsDialog::Id() {
  return id_;
}

void DiagnosticsDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  display_size = gfx::Size(display_size.width() * kDiagnosticsDialogScale,
                           display_size.height() * kDiagnosticsDialogScale);

  *size = display_size;
}

}  // namespace chromeos
