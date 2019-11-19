// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_CLIENTS_H_
#define CHROMEOS_DBUS_SHILL_SHILL_CLIENTS_H_

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace chromeos {
namespace shill_clients {

// Initialize Shill and modemmanager related dbus clients.
COMPONENT_EXPORT(SHILL_CLIENT) void Initialize(dbus::Bus* system_bus);

// Initialize fake Shill and modemmanager related dbus clients.
COMPONENT_EXPORT(SHILL_CLIENT) void InitializeFakes();

// Shut down Shill and modemmanager related dbus clients.
COMPONENT_EXPORT(SHILL_CLIENT) void Shutdown();

}  // namespace shill_clients
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_CLIENTS_H_
