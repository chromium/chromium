// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PCIGUARD_PCIGUARD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PCIGUARD_PCIGUARD_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace dbus {
class Bus;
}

namespace ash {

// PciguardClient is responsible for sending DBus signals to PciGuard daemon.
class COMPONENT_EXPORT(PCIGUARD) PciguardClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnBlockedThunderboltDeviceConnected(
        const std::string& device_name) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

  void NotifyOnBlockedThunderboltDeviceConnected(
      const std::string& device_name);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PCIGUARD_PCIGUARD_CLIENT_H_
