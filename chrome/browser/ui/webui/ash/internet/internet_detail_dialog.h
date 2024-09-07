// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_INTERNET_INTERNET_DETAIL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_INTERNET_INTERNET_DETAIL_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {

class ColorChangeHandler;

}  //  namespace ui

namespace ash {

class NetworkState;

class InternetDetailDialog : public SystemWebDialogDelegate {
 public:
  InternetDetailDialog(const InternetDetailDialog&) = delete;
  InternetDetailDialog& operator=(const InternetDetailDialog&) = delete;

  // Returns whether the dialog is being shown.
  static bool IsShown();

  // Shows an internet details dialog for |network_id|. If no NetworkState
  // exists for |network_id|, does nothing.
  static void ShowDialog(const std::string& network_id,
                         gfx::NativeWindow parent = gfx::NativeWindow());

 protected:
  explicit InternetDetailDialog(const NetworkState& network);
  ~InternetDetailDialog() override;

  // SystemWebDialogDelegate
  std::string Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

 private:
  std::string network_id_;
  std::string network_type_;
  std::string network_name_;
};

class InternetDetailDialogUI;

// WebUIConfig for chrome://internet-detail-dialog
class InternetDetailDialogUIConfig
    : public content::DefaultWebUIConfig<InternetDetailDialogUI> {
 public:
  InternetDetailDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIInternetDetailDialogHost) {}
};

// A WebUI to host a subset of the network details page to allow setting of
// proxy, IP address, and nameservers in the login screen.
class InternetDetailDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit InternetDetailDialogUI(content::WebUI* web_ui);

  InternetDetailDialogUI(const InternetDetailDialogUI&) = delete;
  InternetDetailDialogUI& operator=(const InternetDetailDialogUI&) = delete;

  ~InternetDetailDialogUI() override;

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

  void BindInterface(
      mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
          receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_INTERNET_INTERNET_DETAIL_DIALOG_H_
