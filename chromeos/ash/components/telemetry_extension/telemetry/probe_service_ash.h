// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_TELEMETRY_PROBE_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_TELEMETRY_PROBE_SERVICE_ASH_H_

#include <vector>

#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace ash {

class ProbeServiceAsh : public crosapi::mojom::TelemetryProbeService {
 public:
  ProbeServiceAsh();
  ProbeServiceAsh(const ProbeServiceAsh&) = delete;
  ProbeServiceAsh& operator=(const ProbeServiceAsh&) = delete;
  ~ProbeServiceAsh() override;

 private:
  // crosapi::mojom::TelemetryProbeService override
  void ProbeTelemetryInfo(
      const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_TELEMETRY_PROBE_SERVICE_ASH_H_
