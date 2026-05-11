// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_ash.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace ash {

ProbeServiceAsh::ProbeServiceAsh() = default;

ProbeServiceAsh::~ProbeServiceAsh() = default;

void ProbeServiceAsh::ProbeTelemetryInfo(
    const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          converters::telemetry::ConvertCategoryVector(categories),
          base::BindOnce(
              [](crosapi::mojom::TelemetryProbeService::
                     ProbeTelemetryInfoCallback callback,
                 cros_healthd::mojom::TelemetryInfoPtr ptr) {
                std::move(callback).Run(
                    converters::telemetry::ConvertProbePtr(std::move(ptr)));
              },
              std::move(callback)));
}

}  // namespace ash
