// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_ASH_H_

#include <memory>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class DiagnosticsServiceAsh : public crosapi::mojom::DiagnosticsService {
 public:
  class Factory {
   public:
    static std::unique_ptr<crosapi::mojom::DiagnosticsService> Create(
        mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver);

    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<crosapi::mojom::DiagnosticsService> CreateInstance(
        mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  DiagnosticsServiceAsh();
  DiagnosticsServiceAsh(const DiagnosticsServiceAsh&) = delete;
  DiagnosticsServiceAsh& operator=(const DiagnosticsServiceAsh&) = delete;
  ~DiagnosticsServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver);

  // Ensures that |service_| created and connected to the
  // CrosHealthdDiagnosticsService.
  const mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService>&
  GetService();

 private:
  void OnDisconnect();

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  //
  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::DiagnosticsService> receivers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_ASH_H_
