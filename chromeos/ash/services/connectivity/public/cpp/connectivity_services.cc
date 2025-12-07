// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/connectivity/public/cpp/connectivity_services.h"

#include <utility>

#include "base/check.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_service.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::connectivity {

void BindToPasspointService(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver) {
#if !defined(USE_REAL_DBUS_CLIENTS)
  if (!FakePasspointService::IsInitialized()) {
    FakePasspointService::Initialize();
  }
  FakePasspointService::Get()->BindPendingReceiver(std::move(receiver));
#else
  mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosPasspointService, std::nullopt,
      std::move(receiver).PassPipe());
#endif  // !defined(USE_REAL_DBUS_CLIENTS)
}

}  // namespace ash::connectivity
