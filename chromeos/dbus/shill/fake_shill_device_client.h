// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SHILL_DEVICE_CLIENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_device_client.h"

namespace chromeos {

// A fake implementation of ShillDeviceClient.
// Implemented: Stub cellular device for SMS testing.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillDeviceClient
    : public ShillDeviceClient,
      public ShillDeviceClient::TestInterface {
 public:
  FakeShillDeviceClient();
  ~FakeShillDeviceClient() override;

  // ShillDeviceClient overrides
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(const dbus::ObjectPath& device_path,
                     const DictionaryValueCallback& callback) override;
  void SetProperty(const dbus::ObjectPath& device_path,
                   const std::string& name,
                   const base::Value& value,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;
  void ClearProperty(const dbus::ObjectPath& device_path,
                     const std::string& name,
                     VoidDBusMethodCallback callback) override;
  void RequirePin(const dbus::ObjectPath& device_path,
                  const std::string& pin,
                  bool require,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void EnterPin(const dbus::ObjectPath& device_path,
                const std::string& pin,
                const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void UnblockPin(const dbus::ObjectPath& device_path,
                  const std::string& puk,
                  const std::string& pin,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void ChangePin(const dbus::ObjectPath& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 const base::Closure& callback,
                 const ErrorCallback& error_callback) override;
  void Register(const dbus::ObjectPath& device_path,
                const std::string& network_id,
                const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void Reset(const dbus::ObjectPath& device_path,
             const base::Closure& callback,
             const ErrorCallback& error_callback) override;
  void PerformTDLSOperation(const dbus::ObjectPath& device_path,
                            const std::string& operation,
                            const std::string& peer,
                            const StringCallback& callback,
                            const ErrorCallback& error_callback) override;
  void AddWakeOnPacketConnection(const dbus::ObjectPath& device_path,
                                 const net::IPEndPoint& ip_endpoint,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;
  void AddWakeOnPacketOfTypes(const dbus::ObjectPath& device_path,
                              const std::vector<std::string>& types,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) override;
  void RemoveWakeOnPacketConnection(
      const dbus::ObjectPath& device_path,
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  void RemoveWakeOnPacketOfTypes(const dbus::ObjectPath& device_path,
                                 const std::vector<std::string>& types,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;
  void RemoveAllWakeOnPacketConnections(
      const dbus::ObjectPath& device_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  void SetUsbEthernetMacAddressSource(
      const dbus::ObjectPath& device_path,
      const std::string& source,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

  ShillDeviceClient::TestInterface* GetTestInterface() override;

  // ShillDeviceClient::TestInterface overrides.
  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name) override;
  void RemoveDevice(const std::string& device_path) override;
  void ClearDevices() override;
  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value,
                         bool notify_changed) override;
  std::string GetDevicePathForType(const std::string& type) override;
  void SetTDLSBusyCount(int count) override;
  void SetTDLSState(const std::string& state) override;
  void SetSimLocked(const std::string& device_path, bool locked) override;
  void AddCellularFoundNetwork(const std::string& device_path) override;
  void SetUsbEthernetMacAddressSourceError(
      const std::string& device_path,
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
  void PassStubDeviceProperties(const dbus::ObjectPath& device_path,
                                const DictionaryValueCallback& callback) const;

  // Posts a task to run a void callback with status code |result|.
  void PostVoidCallback(VoidDBusMethodCallback callback, bool result);

  // If |notify_changed| is true, NotifyObserversPropertyChanged is called,
  // otherwise it is not (e.g. when setting up initial properties).
  void SetPropertyInternal(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback,
                           bool notify_changed);

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& device_path,
                                      const std::string& property);
  base::Value* GetDeviceProperties(const std::string& device_path);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);

  // Dictionary of <device_name, Dictionary>.
  base::DictionaryValue stub_devices_;
  // Observer list for each device.
  std::map<dbus::ObjectPath, std::unique_ptr<PropertyObserverList>>
      observer_list_;

  // Number of times to return InProgress for TDLS. Set to -1 to emulate
  // TDLS failure.
  int initial_tdls_busy_count_;

  // Current TDLS busy count.
  int tdls_busy_count_;

  // Fake state for TDLS.
  std::string tdls_state_;

  // Wake on packet connections for each device.
  std::map<dbus::ObjectPath, std::set<net::IPEndPoint>>
      wake_on_packet_connections_;

  // Wake on packet types for each device. The string types in the value set
  // correspond to "Wake on WiFi Packet Type Constants." in
  // third_party/cros_system_api/dbus/shill/dbus-constants.h.
  std::map<dbus::ObjectPath, std::set<std::string>> wake_on_packet_types_;

  // Current SIM PIN per device path.
  std::map<std::string, std::string> sim_pin_;

  // Error names for SetUsbEthernetMacAddressSource error callback for each
  // device. Error callback must not be called if error name is not present or
  // empty.
  std::map<std::string, std::string>
      set_usb_ethernet_mac_address_source_error_names_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillDeviceClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SHILL_DEVICE_CLIENT_H_
