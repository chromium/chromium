// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_ash.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace {

constexpr char kOemDataLogName[] = "oemdata";

}  // namespace

ProbeServiceAsh::ProbeServiceAsh() = default;

ProbeServiceAsh::~ProbeServiceAsh() = default;

void ProbeServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ProbeServiceAsh::ProbeTelemetryInfo(
    const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  GetService()->ProbeTelemetryInfo(
      converters::telemetry::ConvertCategoryVector(categories),
      base::BindOnce(
          [](crosapi::mojom::TelemetryProbeService::ProbeTelemetryInfoCallback
                 callback,
             cros_healthd::mojom::TelemetryInfoPtr ptr) {
            std::move(callback).Run(
                converters::telemetry::ConvertProbePtr(std::move(ptr)));
          },
          std::move(callback)));
}

void ProbeServiceAsh::GetOemData(GetOemDataCallback callback) {
  DebugDaemonClient* debugd_client = DebugDaemonClient::Get();

  debugd_client->GetLog(
      kOemDataLogName,
      base::BindOnce(
          [](GetOemDataCallback callback, std::optional<std::string> oem_data) {
            std::move(callback).Run(
                crosapi::mojom::ProbeOemData::New(std::move(oem_data)));
          },
          std::move(callback)));
}

cros_healthd::mojom::CrosHealthdProbeService* ProbeServiceAsh::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(
        base::BindOnce(&ProbeServiceAsh::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void ProbeServiceAsh::OnDisconnect() {
  service_.reset();
}

}  // namespace ash
