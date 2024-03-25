// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_DEVICE_CLIENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"

namespace ash {

// A fake implementation of ShillDeviceClient.
// Implemented: Stub cellular device for SMS testing.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillDeviceClient
    : public ShillDeviceClient,
      public ShillDeviceClient::TestInterface {
 public:
  FakeShillDeviceClient();

  FakeShillDeviceClient(const FakeShillDeviceClient&) = delete;
  FakeShillDeviceClient& operator=(const FakeShillDeviceClient&) = delete;

  ~FakeShillDeviceClient() override;

  // ShillDeviceClient overrides
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(
      const dbus::ObjectPath& device_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override;
  void SetProperty(const dbus::ObjectPath& device_path,
                   const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override;
  void ClearProperty(const dbus::ObjectPath& device_path,
                     const std::string& name,
                     chromeos::VoidDBusMethodCallback callback) override;
  void RequirePin(const dbus::ObjectPath& device_path,
                  const std::string& pin,
                  bool require,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void EnterPin(const dbus::ObjectPath& device_path,
                const std::string& pin,
                base::OnceClosure callback,
                ErrorCallback error_callback) override;
  void UnblockPin(const dbus::ObjectPath& device_path,
                  const std::string& puk,
                  const std::string& pin,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void ChangePin(const dbus::ObjectPath& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 base::OnceClosure callback,
                 ErrorCallback error_callback) override;
  void Register(const dbus::ObjectPath& device_path,
                const std::string& network_id,
                base::OnceClosure callback,
                ErrorCallback error_callback) override;
  void Reset(const dbus::ObjectPath& device_path,
             base::OnceClosure callback,
             ErrorCallback error_callback) override;
  void SetUsbEthernetMacAddressSource(const dbus::ObjectPath& device_path,
                                      const std::string& source,
                                      base::OnceClosure callback,
                                      ErrorCallback error_callback) override;

  ShillDeviceClient::TestInterface* GetTestInterface() override;

  // ShillDeviceClient::TestInterface overrides.
  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name,
                 const std::string& address) override;
  void RemoveDevice(const std::string& device_path) override;
  void ClearDevices() override;
  base::Value* GetDeviceProperty(const std::string& device_path,
                                 const std::string& name) override;
  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value,
                         bool notify_changed) override;
  std::string GetDevicePathForType(const std::string& type) override;
  void SetSimLocked(const std::string& device_path, bool locked) override;
  void AddCellularFoundNetwork(const std::string& device_path) override;
  void SetUsbEthernetMacAddressSourceError(
      const std::string& device_path,
      const std::string& error_name) override;
  void SetSimulateInhibitScanning(bool simulate_inhibit_scanning) override;
  void SetPropertyChangeDelay(
      std::optional<base::TimeDelta> time_delay) override;
  void SetErrorForNextSetPropertyAttempt(
      const std::string& error_name) override;

  static const char kSimPuk[];
  static const char kDefaultSimPin[];
  static const int kSimPinRetryCount;

 private:
  struct SimLockStatus {
    std::string type = "";
    int retries_left = 0;
    bool lock_enabled = true;
  };
  typedef base::ObserverList<ShillPropertyChangedObserver>::Unchecked
      PropertyObserverList;

  SimLockStatus GetSimLockStatus(const std::string& device_path);
  void SetSimLockStatus(const std::string& device_path,
                        const SimLockStatus& status);
  bool SimTryPin(const std::string& device_path, const std::string& pin);
  bool SimTryPuk(const std::string& device_path, const std::string& pin);
  void PassStubDeviceProperties(
      const dbus::ObjectPath& device_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) const;

  // Posts a task to run a void callback with status code |result|.
  void PostVoidCallback(chromeos::VoidDBusMethodCallback callback, bool result);

  // If |notify_changed| is true, NotifyObserversPropertyChanged is called,
  // otherwise it is not (e.g. when setting up initial properties).
  void SetPropertyInternal(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           base::OnceClosure callback,
                           ErrorCallback error_callback,
                           bool notify_changed);

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& device_path,
                                      const std::string& property);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);

  void SetScanning(const dbus::ObjectPath& device_path, bool is_scanning);

  // Dictionary of <device_name, Dictionary>.
  base::Value::Dict stub_devices_;

  // Observer list for each device.
  std::map<dbus::ObjectPath, std::unique_ptr<PropertyObserverList>>
      observer_list_;

  // Current SIM PIN per device path.
  std::map<std::string, std::string> sim_pin_;

  // Error names for SetUsbEthernetMacAddressSource error callback for each
  // device. Error callback must not be called if error name is not present or
  // empty.
  std::map<std::string, std::string>
      set_usb_ethernet_mac_address_source_error_names_;

  // When true, this class will simulate the inhibit flow by setting the
  // Scanning property to true, then false. This mimics the behavior of Shill
  // during normal operation.
  bool simulate_inhibit_scanning_ = true;

  // When set, causes SetProperty call to return immediately and delay the value
  // change by given amount.
  std::optional<base::TimeDelta> property_change_delay_;

  // If set the next SetProperty call will fail with this error_name.
  std::optional<std::string> set_property_error_name_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillDeviceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_DEVICE_CLIENT_H_
