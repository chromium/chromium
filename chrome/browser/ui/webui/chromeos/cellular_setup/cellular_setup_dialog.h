// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

namespace cellular_setup {

// Dialog which displays the cellular setup flow which allows users to
// activate their un-activated SIM cards. This dialog is only used when the
// kUpdatedCellularActivationUi flag is enabled; see go/cros-cellular-design.
class CellularSetupDialog : public SystemWebDialogDelegate {
 protected:
  CellularSetupDialog();
  ~CellularSetupDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  bool CanResizeDialog() const override;
  void OnDialogClosed(const std::string& json_retval) override;

 private:
  friend void OpenCellularSetupDialog(const std::string& cellular_network_guid);
  static void ShowDialog(const std::string& cellular_network_guid);

  DISALLOW_COPY_AND_ASSIGN(CellularSetupDialog);
};

class CellularSetupDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit CellularSetupDialogUI(content::WebUI* web_ui);
  ~CellularSetupDialogUI() override;

 private:
  void BindCellularSetup(mojo::PendingReceiver<mojom::CellularSetup> receiver);

  DISALLOW_COPY_AND_ASSIGN(CellularSetupDialogUI);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_H_
