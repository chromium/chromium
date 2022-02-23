// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_IMPL_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_IMPL_H_

#include "base/synchronization/lock.h"
#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory.h"
#include "chromeos/services/cros_healthd/private/mojom/cros_healthd_internal.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {

class InternalServiceFactoryImpl
    : public InternalServiceFactory,
      public mojom::CrosHealthdInternalServiceFactory {
 public:
  InternalServiceFactoryImpl();
  InternalServiceFactoryImpl(const InternalServiceFactoryImpl&) = delete;
  InternalServiceFactoryImpl& operator=(const InternalServiceFactoryImpl&) =
      delete;

 public:
  // InternalServiceFactory overrides.
  void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) override;
  void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) override;
  void BindReceiver(
      mojo::PendingReceiver<mojom::CrosHealthdInternalServiceFactory> receiver)
      override;
  // mojom::CrosHealthdInternalServiceFactory overrides.
  void GetNetworkHealthService(NetworkHealthServiceReceiver receiver) override;
  void GetNetworkDiagnosticsRoutines(
      NetworkDiagnosticsRoutinesReceiver receiver) override;

 protected:
  ~InternalServiceFactoryImpl() override;

 private:
  // The receiver set to handle connections.
  mojo::ReceiverSet<mojom::CrosHealthdInternalServiceFactory> receiver_set_;
  // Lock for access from main thread and mojo thread.
  base::Lock lock_;
  // Repeating callbacks that binds a mojo::PendingRemote and returns it.
  BindNetworkHealthServiceCallback bind_network_health_callback_;
  BindNetworkDiagnosticsRoutinesCallback bind_network_diag_callback_;
  // Vectors to keep the pending receivers before the callbacks are ready.
  std::vector<NetworkHealthServiceReceiver> pending_network_health_receivers_;
  std::vector<NetworkDiagnosticsRoutinesReceiver>
      pending_network_diag_receivers_;
};

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_IMPL_H_
