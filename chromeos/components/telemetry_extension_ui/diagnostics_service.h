// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_H_

#if defined(OFFICIAL_BUILD)
#error Diagnostics service should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class DiagnosticsService : public health::mojom::DiagnosticsService {
 public:
  explicit DiagnosticsService(
      mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver);
  DiagnosticsService(const DiagnosticsService&) = delete;
  DiagnosticsService& operator=(const DiagnosticsService&) = delete;
  ~DiagnosticsService() override;

 private:
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        health::mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;

  // Ensures that |service_| created and connected to the
  // CrosHealthdProbeService.
  cros_healthd::mojom::CrosHealthdDiagnosticsService* GetService();

  void OnDisconnect();

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  mojo::Receiver<health::mojom::DiagnosticsService> receiver_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_H_
