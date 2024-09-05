// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_INTERNET_INTERNET_CONFIG_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_INTERNET_INTERNET_CONFIG_DIALOG_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}  //  namespace ui

namespace ash {

class InternetConfigDialog : public SystemWebDialogDelegate {
 public:
  InternetConfigDialog(const InternetConfigDialog&) = delete;
  InternetConfigDialog& operator=(const InternetConfigDialog&) = delete;

  // Shows a network configuration dialog for |network_id|. Does nothing if
  // there is no NetworkState matching |network_id|.
  static void ShowDialogForNetworkId(
      const std::string& network_id,
      gfx::NativeWindow parent = gfx::NativeWindow());
  // Shows a network configuration dialog for a new network of |network_type|.
  static void ShowDialogForNetworkType(
      const std::string& network_type,
      gfx::NativeWindow parent = gfx::NativeWindow());
  // Shows a network configuration dialog with pre-filled Wi-Fi configuration.
  static void ShowDialogForNetworkWithWifiConfig(
      mojo::StructPtr<chromeos::network_config::mojom::WiFiConfigProperties>
          wifi_config,
      gfx::NativeWindow parent = gfx::NativeWindow());

  // SystemWebDialogDelegate
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

 protected:
  // |dialog_id| provides a pre-calculated identifier for the dialog based on
  // the network type and the network id.
  InternetConfigDialog(
      const std::string& dialog_id,
      const std::string& network_type,
      const std::string& network_id,
      std::optional<mojo::StructPtr<
          chromeos::network_config::mojom::WiFiConfigProperties>>
          prefilled_wifi_config);
  ~InternetConfigDialog() override;

  // SystemWebDialogDelegate
  std::string Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

 private:
  std::string dialog_id_;
  std::string network_type_;
  std::string network_id_;
  std::optional<
      mojo::StructPtr<chromeos::network_config::mojom::WiFiConfigProperties>>
      prefilled_wifi_config_;
};

class InternetConfigDialogUI;

// WebUIConfig for chrome://internet-config-dialog
class InternetConfigDialogUIConfig
    : public content::DefaultWebUIConfig<InternetConfigDialogUI> {
 public:
  InternetConfigDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIInternetConfigDialogHost) {}
};

// A WebUI to host the network configuration UI in a dialog, used in the
// login screen and when a new network is configured from the system tray.
class InternetConfigDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit InternetConfigDialogUI(content::WebUI* web_ui);

  InternetConfigDialogUI(const InternetConfigDialogUI&) = delete;
  InternetConfigDialogUI& operator=(const InternetConfigDialogUI&) = delete;

  ~InternetConfigDialogUI() override;

  // Instantiates implementor of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
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

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_INTERNET_INTERNET_CONFIG_DIALOG_H_
