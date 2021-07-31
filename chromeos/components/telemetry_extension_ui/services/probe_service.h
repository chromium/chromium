// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SERVICES_PROBE_SERVICE_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SERVICES_PROBE_SERVICE_H_

#include <vector>

#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class ProbeService : public health::mojom::ProbeService {
 public:
  explicit ProbeService(
      mojo::PendingReceiver<health::mojom::ProbeService> receiver);
  ProbeService(const ProbeService&) = delete;
  ProbeService& operator=(const ProbeService&) = delete;
  ~ProbeService() override;

 private:
  void ProbeTelemetryInfo(
      const std::vector<health::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;
  void GetOemData(GetOemDataCallback callback) override;

  // Ensures that |service_| created and connected to the
  // CrosHealthdProbeService.
  cros_healthd::mojom::CrosHealthdProbeService* GetService();

  void OnDisconnect();

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  mojo::Receiver<health::mojom::ProbeService> receiver_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SERVICES_PROBE_SERVICE_H_
