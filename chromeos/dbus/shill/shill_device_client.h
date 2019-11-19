// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SHILL_DEVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_client_helper.h"

namespace base {
class Value;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace net {
class IPEndPoint;
}  // namespace net

namespace chromeos {

class ShillPropertyChangedObserver;

// ShillDeviceClient is used to communicate with the Shill Device service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillDeviceClient {
 public:
  typedef ShillClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  typedef ShillClientHelper::StringCallback StringCallback;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up devices for testing.
  // Accessed through GetTestInterface(), only implemented in the Stub Impl.
  class TestInterface {
   public:
    virtual void AddDevice(const std::string& device_path,
                           const std::string& type,
                           const std::string& name) = 0;
    virtual void RemoveDevice(const std::string& device_path) = 0;
    virtual void ClearDevices() = 0;
    virtual void SetDeviceProperty(const std::string& device_path,
                                   const std::string& name,
                                   const base::Value& value,
                                   bool notify_changed) = 0;
    virtual std::string GetDevicePathForType(const std::string& type) = 0;
    virtual void SetTDLSBusyCount(int count) = 0;
    virtual void SetTDLSState(const std::string& state) = 0;
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

   protected:
    virtual ~TestInterface() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillDeviceClient* Get();

  // Adds a property changed |observer| for the device at |device_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the device at |device_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call finishes.
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call finishes.
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call finishes.
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             VoidDBusMethodCallback callback) = 0;

  // Calls the RequirePin method.
  // |callback| is called after the method call finishes.
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls the EnterPin method.
  // |callback| is called after the method call finishes.
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

  // Calls the UnblockPin method.
  // |callback| is called after the method call finishes.
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Calls the ChangePin method.
  // |callback| is called after the method call finishes.
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) = 0;

  // Calls the Register method.
  // |callback| is called after the method call finishes.
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;

  // Calls the Reset method.
  // |callback| is called after the method call finishes.
  virtual void Reset(const dbus::ObjectPath& device_path,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) = 0;

  // Calls the PerformTDLSOperation method.
  // |callback| is called after the method call finishes.
  virtual void PerformTDLSOperation(const dbus::ObjectPath& device_path,
                                    const std::string& operation,
                                    const std::string& peer,
                                    const StringCallback& callback,
                                    const ErrorCallback& error_callback) = 0;

  // Adds |ip_endpoint| to the list of tcp connections that the device should
  // monitor to wake the system from suspend.
  virtual void AddWakeOnPacketConnection(
      const dbus::ObjectPath& device_path,
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Adds |types| to the list of packet types that the device should monitor to
  // wake the system from suspend. |types| corresponds to "Wake on WiFi Packet
  // Type Constants." in
  // third_party/cros_system_api/dbus/shill/dbus-constants.h.
  virtual void AddWakeOnPacketOfTypes(const dbus::ObjectPath& device_path,
                                      const std::vector<std::string>& types,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) = 0;

  // Removes |ip_endpoint| from the list of tcp connections that the device
  // should monitor to wake the system from suspend.
  virtual void RemoveWakeOnPacketConnection(
      const dbus::ObjectPath& device_path,
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Removes |types| from the list of packet types that the device should
  // monitor to wake the system from suspend. |types| corresponds to "Wake on
  // WiFi Packet Type Constants." in
  // third_party/cros_system_api/dbus/shill/dbus-constants.h.
  virtual void RemoveWakeOnPacketOfTypes(
      const dbus::ObjectPath& device_path,
      const std::vector<std::string>& types,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Clears the list of tcp connections that the device should monitor to wake
  // the system from suspend.
  virtual void RemoveAllWakeOnPacketConnections(
      const dbus::ObjectPath& device_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Set MAC address source for USB Ethernet adapter. |source| corresponds to
  // "USB Ethernet MAC address sources." in
  // third_party/cros_system_api/dbus/shill/dbus-constants.h.
  virtual void SetUsbEthernetMacAddressSource(
      const dbus::ObjectPath& device_path,
      const std::string& source,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillDeviceClientTest;

  // Initialize/Shutdown should be used instead.
  ShillDeviceClient();
  virtual ~ShillDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_DEVICE_CLIENT_H_
