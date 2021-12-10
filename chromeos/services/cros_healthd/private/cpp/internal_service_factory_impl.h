// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_IMPL_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_IMPL_H_

#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory.h"
#include "chromeos/services/cros_healthd/private/mojom/cros_healthd_internal.mojom.h"

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
};

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PRIVATE_CPP_INTERNAL_SERVICE_FACTORY_IMPL_H_
