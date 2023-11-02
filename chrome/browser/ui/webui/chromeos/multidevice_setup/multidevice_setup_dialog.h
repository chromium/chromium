// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_H_

#include <string>
#include <vector>

#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "base/callback.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

namespace multidevice_setup {

// Dialog which displays the multi-device setup flow which allows users to
// enable features involving communication between multiple devices (e.g., a
// Chromebook and a phone).
class MultiDeviceSetupDialog : public SystemWebDialogDelegate {
 public:
  MultiDeviceSetupDialog(const MultiDeviceSetupDialog&) = delete;
  MultiDeviceSetupDialog& operator=(const MultiDeviceSetupDialog&) = delete;

  // Shows the dialog; if the dialog is already displayed, this function is a
  // no-op.
  static void Show();

  // Returns the currently displayed dialog. If no dialog exists, returns
  // nullptr.
  static MultiDeviceSetupDialog* Get();

  static void SetInstanceForTesting(MultiDeviceSetupDialog* instance);

  // Registers a callback which will be called when the dialog is closed.
  void AddOnCloseCallback(base::OnceClosure callback);

 protected:
  MultiDeviceSetupDialog();
  ~MultiDeviceSetupDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  void OnDialogClosed(const std::string& json_retval) override;

 private:
  static MultiDeviceSetupDialog* current_instance_;
  static gfx::NativeWindow containing_window_;

  // SystemWebDialogDelegate:
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

  // List of callbacks that have registered themselves to be invoked once this
  // dialog is closed.
  std::vector<base::OnceClosure> on_close_callbacks_;
};

class MultiDeviceSetupDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit MultiDeviceSetupDialogUI(content::WebUI* web_ui);

  MultiDeviceSetupDialogUI(const MultiDeviceSetupDialogUI&) = delete;
  MultiDeviceSetupDialogUI& operator=(const MultiDeviceSetupDialogUI&) = delete;

  ~MultiDeviceSetupDialogUI() override;

  // Instantiates implementor of the mojom::MultiDeviceSetup mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ash::multidevice_setup::mojom::MultiDeviceSetup>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace multidevice_setup

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace multidevice_setup {
using ::chromeos::multidevice_setup::MultiDeviceSetupDialog;
}
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_H_
