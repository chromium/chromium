// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/diagnostics_dialog.h"

#include <string>

#include "chromeos/components/diagnostics_ui/url_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {

// Scale factor for size of the diagnostics dialog, based on display size.
const float kDiagnosticsDialogScale = .8;

// static
void DiagnosticsDialog::ShowDialog() {
  DiagnosticsDialog* dialog = new DiagnosticsDialog();
  dialog->ShowSystemDialog();
}

DiagnosticsDialog::DiagnosticsDialog()
    : SystemWebDialogDelegate(GURL(kChromeUIDiagnosticsAppUrl),
                              /*title=*/std::u16string()) {}

DiagnosticsDialog::~DiagnosticsDialog() = default;

const std::string& DiagnosticsDialog::Id() {
  return id_;
}

void DiagnosticsDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  display_size = gfx::Size(display_size.width() * kDiagnosticsDialogScale,
                           display_size.height() * kDiagnosticsDialogScale);

  *size = display_size;
}

}  // namespace chromeos
