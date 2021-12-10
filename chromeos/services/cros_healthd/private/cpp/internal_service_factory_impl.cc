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
    BindNetworkHealthServiceCallback callback) {}

void InternalServiceFactoryImpl::SetBindNetworkDiagnosticsRoutinesCallback(
    BindNetworkDiagnosticsRoutinesCallback callback) {}

void InternalServiceFactoryImpl::BindReceiver(
    mojo::PendingReceiver<mojom::CrosHealthdInternalServiceFactory> receiver) {}

void InternalServiceFactoryImpl::GetNetworkHealthService(
    NetworkHealthServiceReceiver receiver) {}

void InternalServiceFactoryImpl::GetNetworkDiagnosticsRoutines(
    NetworkDiagnosticsRoutinesReceiver receiver) {}

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
