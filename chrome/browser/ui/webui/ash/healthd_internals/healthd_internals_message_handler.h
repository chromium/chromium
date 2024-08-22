// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_HEALTHD_INTERNALS_HEALTHD_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_HEALTHD_INTERNALS_HEALTHD_INTERNALS_MESSAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class HealthdInternalsMessageHandler : public content::WebUIMessageHandler {
 public:
  HealthdInternalsMessageHandler();

  HealthdInternalsMessageHandler(const HealthdInternalsMessageHandler&) =
      delete;
  HealthdInternalsMessageHandler& operator=(
      const HealthdInternalsMessageHandler&) = delete;

  ~HealthdInternalsMessageHandler() override;

  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;

 private:
  // Handle the `getHealthdInternalsFeatureFlag` request.
  void HandleGetHealthdInternalsFeatureFlag(const base::Value::List& args);

  // Handle the `getHealthdTelemetryInfo` request.
  void HandleGetHealthdTelemetryInfo(const base::Value::List& args);

  // Handle the `getHealthdProcessInfo` request.
  void HandleGetHealthdProcessInfo(const base::Value::List& args);

  // Handle the telemetry result from `probe_service_`.
  void HandleTelemetryResult(base::Value callback_id,
                             cros_healthd::mojom::TelemetryInfoPtr info);

  // Handle the process result from `probe_service_`.
  void HandleMultipleProcessResult(
      base::Value callback_id,
      cros_healthd::mojom::MultipleProcessResultPtr process_result);

  // Reply the `getHealthdTelemetryInfo` and `getHealthdProcessInfo` requests.
  void ReplyHealthdInternalInfo(base::Value callback_id,
                                base::Value::Dict result);

  // Ensures that `probe_service_` created and connected to the
  // `CrosHealthdProbeService`.
  cros_healthd::mojom::CrosHealthdProbeService* GetProbeService();

  // Handle the disconnect of `probe_service_`.
  void OnProbeServiceDisconnect();

 private:
  // The telemetry service from healthd.
  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> probe_service_;

  // Must be the last member.
  base::WeakPtrFactory<HealthdInternalsMessageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_HEALTHD_INTERNALS_HEALTHD_INTERNALS_MESSAGE_HANDLER_H_
