// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_
#define CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

class ConnectivityDiagnosticsUI : public ui::MojoWebUIController {
 public:
  using BindNetworkDiagnosticsServiceCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines>)>;

  using BindNetworkHealthServiceCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<network_health::mojom::NetworkHealthService>)>;

  using SendFeedbackReportCallback =
      base::RepeatingCallback<void(const std::string& extra_diagnostics)>;

  explicit ConnectivityDiagnosticsUI(
      content::WebUI* web_ui,
      BindNetworkDiagnosticsServiceCallback bind_network_diagnostics_callback,
      BindNetworkHealthServiceCallback bind_network_health_callback,
      SendFeedbackReportCallback send_feeback_report_callback);
  ~ConnectivityDiagnosticsUI() override;
  ConnectivityDiagnosticsUI(const ConnectivityDiagnosticsUI&) = delete;
  ConnectivityDiagnosticsUI& operator=(const ConnectivityDiagnosticsUI&) =
      delete;

  // Instantiates implementation of the mojom::NetworkDiagnosticsRoutines mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<
          network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver);

  // Instantiates implementation of the mojom::NetworkHealthService mojo
  // interface passing the pending receiver that will be bound.
  void BindInterface(
      mojo::PendingReceiver<network_health::mojom::NetworkHealthService>
          receiver);

  void SendFeedbackReportRequest(const base::ListValue* value);

 private:
  const BindNetworkDiagnosticsServiceCallback
      bind_network_diagnostics_service_callback_;

  const BindNetworkHealthServiceCallback bind_network_health_service_callback_;

  const SendFeedbackReportCallback send_feedback_report_callback_;

  base::WeakPtrFactory<ConnectivityDiagnosticsUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_
