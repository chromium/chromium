// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UI_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UI_H_

#if defined(OFFICIAL_BUILD)
#error Telemetry Extension should only be included in unofficial builds.
#endif

#include "memory"

#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom-forward.h"
#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom-forward.h"
#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// The WebUI for chrome://telemetry-extension/.
class TelemetryExtensionUI : public ui::MojoWebUIController {
 public:
  explicit TelemetryExtensionUI(content::WebUI* web_ui);
  TelemetryExtensionUI(const TelemetryExtensionUI&) = delete;
  TelemetryExtensionUI& operator=(const TelemetryExtensionUI&) = delete;
  ~TelemetryExtensionUI() override;

  void BindInterface(
      mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver);

  void BindInterface(
      mojo::PendingReceiver<health::mojom::ProbeService> receiver);

  void BindInterface(
      mojo::PendingReceiver<health::mojom::SystemEventsService> receiver);

 private:
  // Replaced when |BindInterface| is called.
  std::unique_ptr<health::mojom::DiagnosticsService> diagnostics_service_;
  std::unique_ptr<health::mojom::ProbeService> probe_service_;
  std::unique_ptr<health::mojom::SystemEventsService> system_events_service_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UI_H_
