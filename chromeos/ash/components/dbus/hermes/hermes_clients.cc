// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"

#include "chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"

namespace ash::hermes_clients {

void Initialize(dbus::Bus* system_bus) {
#if !defined(USE_REAL_DBUS_CLIENTS)
  if (!system_bus)
    return InitializeFakes();
#endif
  DCHECK(system_bus);
  // The Hermes fake Manager client depends on fake Profile client
  // to coordinate creating and managing of fake profile objects. The
  // following makes sure that they are initialized in the correct order.
  HermesProfileClient::Initialize(system_bus);
  HermesEuiccClient::Initialize(system_bus);
  HermesManagerClient::Initialize(system_bus);
}

void InitializeFakes() {
  HermesProfileClient::InitializeFake();
  HermesEuiccClient::InitializeFake();
  HermesManagerClient::InitializeFake();
}

void Shutdown() {
  HermesManagerClient::Shutdown();
  HermesEuiccClient::Shutdown();
  HermesProfileClient::Shutdown();
}

}  // namespace ash::hermes_clients
