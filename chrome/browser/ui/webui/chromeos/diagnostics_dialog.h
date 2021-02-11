// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos {

class DiagnosticsDialog : public SystemWebDialogDelegate {
 public:
  static void ShowDialog();

 protected:
  DiagnosticsDialog();
  ~DiagnosticsDialog() override;

  DiagnosticsDialog(const DiagnosticsDialog&) = delete;
  DiagnosticsDialog& operator=(const DiagnosticsDialog&) = delete;

  // SystemWebDialogDelegate
  const std::string& Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;

 private:
  const std::string id_ = "diagnostics-dialog";
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIALOG_H_
