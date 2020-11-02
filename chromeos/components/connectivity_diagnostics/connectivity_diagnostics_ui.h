// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_
#define CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_

#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

class ConnectivityDiagnosticsUI : public ui::MojoWebUIController {
 public:
  using BindNetworkDiagnosticsServiceCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines>)>;

  explicit ConnectivityDiagnosticsUI(
      content::WebUI* web_ui,
      BindNetworkDiagnosticsServiceCallback bind_network_diagnostics_callback);
  ~ConnectivityDiagnosticsUI() override;
  ConnectivityDiagnosticsUI(const ConnectivityDiagnosticsUI&) = delete;
  ConnectivityDiagnosticsUI& operator=(const ConnectivityDiagnosticsUI&) =
      delete;

  // Instantiates implementation of the mojom::NetworkDiagnosticsRoutines mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver);

 private:
  const BindNetworkDiagnosticsServiceCallback
      bind_network_diagnostics_service_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_
