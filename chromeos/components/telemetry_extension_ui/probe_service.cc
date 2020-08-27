// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/probe_service.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/components/telemetry_extension_ui/convert_ptr.h"
#include "chromeos/components/telemetry_extension_ui/probe_service_converters.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {

ProbeService::ProbeService(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver)
    : receiver_(this, std::move(receiver)) {}

ProbeService::~ProbeService() = default;

void ProbeService::ProbeTelemetryInfo(
    const std::vector<health::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  GetService()->ProbeTelemetryInfo(
      converters::ConvertCategoryVector(categories),
      base::BindOnce(
          [](health::mojom::ProbeService::ProbeTelemetryInfoCallback callback,
             cros_healthd::mojom::TelemetryInfoPtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

cros_healthd::mojom::CrosHealthdProbeService* ProbeService::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(
        base::BindOnce(&ProbeService::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void ProbeService::OnDisconnect() {
  service_.reset();
}

}  // namespace chromeos
