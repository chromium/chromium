// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PCIGUARD_PCIGUARD_CLIENT_H_
#define CHROMEOS_DBUS_PCIGUARD_PCIGUARD_CLIENT_H_

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// PciguardClient is responsible for sending DBus signals to PciGuard daemon.
class COMPONENT_EXPORT(PCIGUARD) PciguardClient {
 public:
  // Pciguard daemon D-Bus method call. See
  // third_party/cros_system_api/dbus/pciguard/dbus-constants.h for D-Bus
  // constant definitions.
  virtual void SendExternalPciDevicesPermissionState(bool permitted) = 0;

  // Creates and initializes a global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static PciguardClient* Get();

 protected:
  // Initialize/Shutdown should be used instead.
  PciguardClient();

  PciguardClient(const PciguardClient&) = delete;
  PciguardClient& operator=(const PciguardClient&) = delete;
  virtual ~PciguardClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PCIGUARD_PCIGUARD_CLIENT_H_
