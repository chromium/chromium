// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CHROMEBOX_FOR_MEETINGS_NETWORK_SETTINGS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CHROMEBOX_FOR_MEETINGS_NETWORK_SETTINGS_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash::cfm {

// Dialog for displaying network setting options on CFM.
class NetworkSettingsDialog : public SystemWebDialogDelegate {
 public:
  static bool IsShown();
  static void ShowDialog();

 protected:
  NetworkSettingsDialog();

  NetworkSettingsDialog(const NetworkSettingsDialog&) = delete;
  NetworkSettingsDialog& operator=(const NetworkSettingsDialog&) = delete;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  void OnDialogClosed(const std::string& json_retval) override;
};

// UI controller for contents of dialog.
class NetworkSettingsDialogUi : public ui::MojoWebDialogUI {
 public:
  explicit NetworkSettingsDialogUi(content::WebUI* web_ui);

  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CHROMEBOX_FOR_MEETINGS_NETWORK_SETTINGS_DIALOG_H_
