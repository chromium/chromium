// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_UI_H_

#include "base/macros.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// WebUI controller for chrome://network debugging page.
class NetworkUI : public ui::MojoWebUIController {
 public:
  explicit NetworkUI(content::WebUI* web_ui);
  ~NetworkUI() override;

  static void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Instantiates implementation of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver);

  // Instantiates implementation of the mojom::NetworkHealthService mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<network_health::mojom::NetworkHealthService>
          receiver);

  // Instantiates implementation of the mojom::NetworkDiagnosticsRoutines mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver);

  // Instantiates implementor of the mojom::ESimManager mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(NetworkUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_UI_H_
