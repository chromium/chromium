// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_TYPECD_TYPECD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_TYPECD_TYPECD_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace dbus {
class Bus;
}

namespace ash {

// TypecdClient is responsible receiving D-bus signals from the TypeCDaemon
// service. The TypeCDaemon is the underlying service that informs us whenever
// a new Thunderbolt/USB4 peripheral has been plugged in.
class COMPONENT_EXPORT(TYPECD) TypecdClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnThunderboltDeviceConnected(bool is_thunderbolt_only) = 0;
    virtual void OnCableWarning(
        typecd::CableWarningType cable_warning_type) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Creates and initializes a global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static TypecdClient* Get();

  // Calls `typecd` to set whether peripheral data access is perimitted.
  virtual void SetPeripheralDataAccessPermissionState(bool permitted) = 0;

  // Calls `typecd` to set which ports are used for displays.
  virtual void SetTypeCPortsUsingDisplays(
      const std::vector<uint32_t>& port_nums) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  TypecdClient();

  TypecdClient(const TypecdClient&) = delete;
  TypecdClient& operator=(const TypecdClient&) = delete;
  virtual ~TypecdClient();

  void NotifyOnThunderboltDeviceConnected(bool is_thunderbolt_only);
  void NotifyOnCableWarning(typecd::CableWarningType cable_warning_type);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_TYPECD_TYPECD_CLIENT_H_
