// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory_impl.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {

InternalServiceFactoryImpl::InternalServiceFactoryImpl() = default;

InternalServiceFactoryImpl::~InternalServiceFactoryImpl() = default;

void InternalServiceFactoryImpl::SetBindNetworkHealthServiceCallback(
    BindNetworkHealthServiceCallback callback) {
  DCHECK(!callback.is_null());
  base::AutoLock lock(lock_);
  bind_network_health_callback_ = std::move(callback);
  for (auto& receiver : pending_network_health_receivers_) {
    bind_network_health_callback_.Run(std::move(receiver));
  }
  pending_network_health_receivers_.clear();
}

void InternalServiceFactoryImpl::SetBindNetworkDiagnosticsRoutinesCallback(
    BindNetworkDiagnosticsRoutinesCallback callback) {
  DCHECK(!callback.is_null());
  base::AutoLock lock(lock_);
  bind_network_diag_callback_ = std::move(callback);
  for (auto& receiver : pending_network_diag_receivers_) {
    bind_network_diag_callback_.Run(std::move(receiver));
  }
  pending_network_diag_receivers_.clear();
}

void InternalServiceFactoryImpl::BindReceiver(
    mojo::PendingReceiver<mojom::CrosHealthdInternalServiceFactory> receiver) {
  receiver_set_.Add(/*impl=*/this, std::move(receiver));
}

void InternalServiceFactoryImpl::GetNetworkHealthService(
    NetworkHealthServiceReceiver receiver) {
  base::AutoLock lock(lock_);
  if (bind_network_health_callback_.is_null()) {
    pending_network_health_receivers_.push_back(std::move(receiver));
    return;
  }
  bind_network_health_callback_.Run(std::move(receiver));
}

void InternalServiceFactoryImpl::GetNetworkDiagnosticsRoutines(
    NetworkDiagnosticsRoutinesReceiver receiver) {
  base::AutoLock lock(lock_);
  if (bind_network_diag_callback_.is_null()) {
    pending_network_diag_receivers_.push_back(std::move(receiver));
    return;
  }
  bind_network_diag_callback_.Run(std::move(receiver));
}

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
