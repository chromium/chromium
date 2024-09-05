// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_BLUETOOTH_BLUETOOTH_PAIRING_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_BLUETOOTH_BLUETOOTH_PAIRING_DIALOG_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace color_change_listener::mojom {
class PageHandler;
}  // namespace color_change_listener::mojom

namespace ui {
class ColorChangeHandler;
}  //  namespace ui

namespace ash {

class BluetoothPairingDialog : public SystemWebDialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBluetoothPairingDialogElementId);

  BluetoothPairingDialog(const BluetoothPairingDialog&) = delete;
  BluetoothPairingDialog& operator=(const BluetoothPairingDialog&) = delete;

  // Show the Bluetooth pairing dialog. When provided, |device_address| is the
  // unique device address that the dialog should attempt to pair with and
  // should be in the form "XX:XX:XX:XX:XX:XX". When |device_address| is not
  // provided the dialog will show the device list instead. The returned object
  // manages its own lifetime, for more information see
  // chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h.
  static SystemWebDialogDelegate* ShowDialog(
      std::optional<std::string_view> device_address = std::nullopt);

  ~BluetoothPairingDialog() override;

 protected:
  BluetoothPairingDialog(
      const std::string& dialog_id,
      std::optional<std::string_view> canonical_device_address);

 private:
  // SystemWebDialogDelegate:
  std::string Id() override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

  // The canonical Bluetooth address of a device when pairing a specific device,
  // otherwise |kChromeUIBluetoothPairingURL|.
  std::string dialog_id_;
};

class BluetoothPairingDialogUI;

// WebUIConfig for chrome://bluetooth-pairing
class BluetoothPairingDialogUIConfig
    : public content::DefaultWebUIConfig<BluetoothPairingDialogUI> {
 public:
  BluetoothPairingDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBluetoothPairingHost) {}
};

// A WebUI to host the Bluetooth device pairing web UI.
class BluetoothPairingDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit BluetoothPairingDialogUI(content::WebUI* web_ui);

  BluetoothPairingDialogUI(const BluetoothPairingDialogUI&) = delete;
  BluetoothPairingDialogUI& operator=(const BluetoothPairingDialogUI&) = delete;

  ~BluetoothPairingDialogUI() override;

  // Instantiates implementor of the mojom::CrosBluetoothConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ash::bluetooth_config::mojom::CrosBluetoothConfig>
          receiver);

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_change_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_BLUETOOTH_BLUETOOTH_PAIRING_DIALOG_H_
