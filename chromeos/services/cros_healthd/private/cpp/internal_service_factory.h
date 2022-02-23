// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_H_

#include <mojo/public/cpp/bindings/pending_receiver.h>

#include "chromeos/services/cros_healthd/private/mojom/cros_healthd_internal.mojom-forward.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {

class InternalServiceFactory {
 public:
  static InternalServiceFactory* GetInstance();

  using NetworkHealthServiceReceiver = mojo::PendingReceiver<
      chromeos::network_health::mojom::NetworkHealthService>;
  using BindNetworkHealthServiceCallback =
      base::RepeatingCallback<void(NetworkHealthServiceReceiver)>;

  using NetworkDiagnosticsRoutinesReceiver = mojo::PendingReceiver<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>;
  using BindNetworkDiagnosticsRoutinesCallback =
      base::RepeatingCallback<void(NetworkDiagnosticsRoutinesReceiver)>;

  // Sets a callback to request binding a PendingReceiver to the
  // NetworkHealthService. This callback is invoked when cros_healthd connects.
  virtual void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) = 0;
  // Sets a callback to request binding a PendingReceiver to the
  // NetworkDiagnosticsRoutines interface. This callback is invoked when
  // cros_healthd connects.
  virtual void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) = 0;
  // Binds a mojo pending receiver to this service.
  virtual void BindReceiver(
      mojo::PendingReceiver<mojom::CrosHealthdInternalServiceFactory>
          receiver) = 0;

 protected:
  InternalServiceFactory() = default;
  InternalServiceFactory(const InternalServiceFactory&) = delete;
  InternalServiceFactory& operator=(const InternalServiceFactory&) = delete;
  virtual ~InternalServiceFactory() = default;
};

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace cros_healthd {
namespace internal {
using ::chromeos::cros_healthd::internal::InternalServiceFactory;
}
}  // namespace cros_healthd
}  // namespace ash

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_H_
