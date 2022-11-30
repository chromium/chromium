// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_CLIENTS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_CLIENTS_H_

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace ash::hermes_clients {

// Initializes all Hermes dbus clients in the correct order.
COMPONENT_EXPORT(HERMES_CLIENT) void Initialize(dbus::Bus* system_bus);

// Initializes fake implementations of all Hermes dbus clients.
COMPONENT_EXPORT(HERMES_CLIENT) void InitializeFakes();

// Shutdown all Hermes dbus clients.
COMPONENT_EXPORT(HERMES_CLIENT) void Shutdown();

}  // namespace ash::hermes_clients

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace chromeos::hermes_clients {
using ::ash::hermes_clients::InitializeFakes;
using ::ash::hermes_clients::Shutdown;
}  // namespace chromeos::hermes_clients

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_CLIENTS_H_
