// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_NETWORK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_NETWORK_UI_H_

#include "base/values.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class NetworkUI;

// WebUIConfig for chrome://network
class NetworkUIConfig : public content::DefaultWebUIConfig<NetworkUI> {
 public:
  NetworkUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINetworkHost) {}
};

// WebUI controller for chrome://network debugging page.
class NetworkUI : public ui::MojoWebUIController {
 public:
  explicit NetworkUI(content::WebUI* web_ui);

  NetworkUI(const NetworkUI&) = delete;
  NetworkUI& operator=(const NetworkUI&) = delete;

  ~NetworkUI() override;

  static base::Value::Dict GetLocalizedStrings();

  // Instantiates implementation of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

  // Instantiates implementation of the mojom::NetworkHealthService mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<
          chromeos::network_health::mojom::NetworkHealthService> receiver);

  // Instantiates implementation of the mojom::NetworkDiagnosticsRoutines mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          receiver);

  // Instantiates implementor of the mojom::ESimManager mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver);

  // Instantiates the implementation of mojom::PasspointService mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_NETWORK_UI_NETWORK_UI_H_
