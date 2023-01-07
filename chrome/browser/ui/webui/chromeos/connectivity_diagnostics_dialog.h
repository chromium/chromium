// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CONNECTIVITY_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CONNECTIVITY_DIAGNOSTICS_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos {

class ConnectivityDiagnosticsDialog : public SystemWebDialogDelegate {
 public:
  static void ShowDialog();

 protected:
  ConnectivityDiagnosticsDialog();
  ~ConnectivityDiagnosticsDialog() override;

  ConnectivityDiagnosticsDialog(const ConnectivityDiagnosticsDialog&) = delete;
  ConnectivityDiagnosticsDialog& operator=(
      const ConnectivityDiagnosticsDialog&) = delete;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CONNECTIVITY_DIAGNOSTICS_DIALOG_H_
