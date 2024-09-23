// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

// HermesManagerClient is used to talk to the main Hermes Manager dbus object.
class COMPONENT_EXPORT(HERMES_CLIENT) HermesManagerClient {
 public:
  class TestInterface {
   public:
    // Adds a new Euicc object with given path and properties.
    virtual void AddEuicc(const dbus::ObjectPath& path,
                          const std::string& eid,
                          bool is_active,
                          uint32_t physical_slot) = 0;

    // Clears all Euicc objects and associated profiles.
    virtual void ClearEuiccs() = 0;
  };

  // Interface for observing Hermes Manager changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new Euicc objects are added or removed.
    virtual void OnAvailableEuiccListChanged() {}

    // Called when the Hermes clients are being shut down.
    virtual void OnShutdown() {}
  };

  // Adds an observer for carrier profile lists changes on Hermes manager.
  virtual void AddObserver(Observer* observer);

  // Removes an observer for Hermes manager.
  virtual void RemoveObserver(Observer* observer);

  // Returns the list of all installed Euiccs.
  virtual const std::vector<dbus::ObjectPath>& GetAvailableEuiccs() = 0;

  // Returns an instance of Hermes Manager Test interface.
  virtual TestInterface* GetTestInterface() = 0;

  // Creates and initializes the global instance.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a global fake instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance.
  static HermesManagerClient* Get();

 protected:
  HermesManagerClient();
  virtual ~HermesManagerClient();

  const base::ObserverList<HermesManagerClient::Observer>::Unchecked&
  observers() {
    return observers_;
  }

 private:
  base::ObserverList<HermesManagerClient::Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_MANAGER_CLIENT_H_
