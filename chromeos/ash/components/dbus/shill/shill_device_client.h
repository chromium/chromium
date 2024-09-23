// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_DEVICE_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/shill/shill_client_helper.h"

namespace base {
class TimeDelta;
class Value;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace ash {

class ShillPropertyChangedObserver;

// ShillDeviceClient is used to communicate with the Shill Device service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillDeviceClient {
 public:
  typedef ShillClientHelper::StringCallback StringCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up devices for testing.
  // Accessed through GetTestInterface(), only implemented in the Stub Impl.
  class TestInterface {
   public:
    virtual void AddDevice(const std::string& device_path,
                           const std::string& type,
                           const std::string& name,
                           const std::string& address = "") = 0;
    virtual void RemoveDevice(const std::string& device_path) = 0;
    virtual void ClearDevices() = 0;
    virtual base::Value* GetDeviceProperty(const std::string& device_path,
                                           const std::string& name) = 0;
    virtual void SetDeviceProperty(const std::string& device_path,
                                   const std::string& name,
                                   const base::Value& value,
                                   bool notify_changed) = 0;
    virtual std::string GetDevicePathForType(const std::string& type) = 0;
    // If |lock_type| is true, sets Cellular.SIMLockStatus.LockType to sim-pin,
    // otherwise clears LockType. (This will unblock a PUK locked SIM).
    // Sets RetriesLeft to the PIN retry default. LockEnabled is unaffected.
    virtual void SetSimLocked(const std::string& device_path, bool enabled) = 0;
    // Adds a new entry to Cellular.FoundNetworks.
    virtual void AddCellularFoundNetwork(const std::string& device_path) = 0;
    // Sets error for SetUsbEthernetMacAddressSourceError error callback. Error
    // callback must be called only if |error_name| is not empty.
    virtual void SetUsbEthernetMacAddressSourceError(
        const std::string& device_path,
        const std::string& error_name) = 0;
    // Determines whether or not to simulate the Scanning property changing when
    // an Inhibit property is updated.
    virtual void SetSimulateInhibitScanning(bool simulate_inhibit_scanning) = 0;
    // Adds a delay before a SetProperty call will result in property value
    // change.
    virtual void SetPropertyChangeDelay(
        std::optional<base::TimeDelta> time_delay) = 0;
    // Sets a SetProperty error. If set, the next SetProperty call will
    // fail with the given |error_name|
    virtual void SetErrorForNextSetPropertyAttempt(
        const std::string& error_name) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillDeviceClient* Get();

  ShillDeviceClient(const ShillDeviceClient&) = delete;
  ShillDeviceClient& operator=(const ShillDeviceClient&) = delete;

  // Adds a property changed |observer| for the device at |device_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the device at |device_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls the GetProperties DBus method and invokes |callback| when complete.
  // |callback| receives a dictionary Value containing the Device properties on
  // success or nullopt on failure.
  virtual void GetProperties(
      const dbus::ObjectPath& device_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call finishes.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call finishes.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             chromeos::VoidDBusMethodCallback callback) = 0;

  // Calls the RequirePin method.
  // |callback| is called after the method call finishes.
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Calls the EnterPin method.
  // |callback| is called after the method call finishes.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        base::OnceClosure callback,
                        ErrorCallback error_callback) = 0;

  // Calls the UnblockPin method.
  // |callback| is called after the method call finishes.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Calls the ChangePin method.
  // |callback| is called after the method call finishes.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) = 0;

  // Calls the Register method.
  // |callback| is called after the method call finishes.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        base::OnceClosure callback,
                        ErrorCallback error_callback) = 0;

  // Calls the Reset method.
  // |callback| is called after the method call finishes.
  virtual void Reset(const dbus::ObjectPath& device_path,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) = 0;

  // Set MAC address source for USB Ethernet adapter. |source| corresponds to
  // "USB Ethernet MAC address sources." in
  // third_party/cros_system_api/dbus/shill/dbus-constants.h.
  virtual void SetUsbEthernetMacAddressSource(
      const dbus::ObjectPath& device_path,
      const std::string& source,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillDeviceClientTest;

  // Initialize/Shutdown should be used instead.
  ShillDeviceClient();
  virtual ~ShillDeviceClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_DEVICE_CLIENT_H_
