// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_device_client.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kSimPuk[] = "12345678";  // Matches pseudomodem.
const int kSimPinMinLength = 4;
const int kSimPukRetryCount = 10;
const char kFailedMessage[] = "Failed";

void ErrorFunction(const std::string& device_path,
                   const std::string& error_name,
                   const std::string& error_message) {
  LOG(ERROR) << "Shill Error for: " << device_path << ": " << error_name
             << " : " << error_message;
}

void PostError(const std::string& error,
               const ShillDeviceClient::ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(error_callback, error, kFailedMessage));
}

void PostNotFoundError(const ShillDeviceClient::ErrorCallback& error_callback) {
  PostError(shill::kErrorResultNotFound, error_callback);
}

bool IsReadOnlyProperty(const std::string& name) {
  if (name == shill::kCarrierProperty)
    return true;
  return false;
}

}  // namespace

const char FakeShillDeviceClient::kDefaultSimPin[] = "1111";
const int FakeShillDeviceClient::kSimPinRetryCount = 3;

FakeShillDeviceClient::FakeShillDeviceClient()
    : initial_tdls_busy_count_(0),
      tdls_busy_count_(0),
      weak_ptr_factory_(this) {}

FakeShillDeviceClient::~FakeShillDeviceClient() = default;

// ShillDeviceClient overrides.

void FakeShillDeviceClient::Init(dbus::Bus* bus) {}

void FakeShillDeviceClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(device_path).AddObserver(observer);
}

void FakeShillDeviceClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(device_path).RemoveObserver(observer);
}

void FakeShillDeviceClient::GetProperties(
    const dbus::ObjectPath& device_path,
    const DictionaryValueCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillDeviceClient::PassStubDeviceProperties,
                     weak_ptr_factory_.GetWeakPtr(), device_path, callback));
}

void FakeShillDeviceClient::SetProperty(const dbus::ObjectPath& device_path,
                                        const std::string& name,
                                        const base::Value& value,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  if (IsReadOnlyProperty(name))
    PostError(shill::kErrorResultInvalidArguments, error_callback);
  SetPropertyInternal(device_path, name, value, callback, error_callback,
                      /*notify_changed=*/true);
}

void FakeShillDeviceClient::SetPropertyInternal(
    const dbus::ObjectPath& device_path,
    const std::string& name,
    const base::Value& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool notify_changed) {
  base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(device_path.value(),
                                                       &device_properties)) {
    PostNotFoundError(error_callback);
    return;
  }
  device_properties->SetKey(name, value.Clone());
  if (notify_changed) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeShillDeviceClient::NotifyObserversPropertyChanged,
                       weak_ptr_factory_.GetWeakPtr(), device_path, name));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::ClearProperty(const dbus::ObjectPath& device_path,
                                          const std::string& name,
                                          VoidDBusMethodCallback callback) {
  base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(device_path.value(),
                                                       &device_properties)) {
    PostVoidCallback(std::move(callback), false);
    return;
  }
  device_properties->RemoveWithoutPathExpansion(name, NULL);
  PostVoidCallback(std::move(callback), true);
}

void FakeShillDeviceClient::RequirePin(const dbus::ObjectPath& device_path,
                                       const std::string& pin,
                                       bool require,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  VLOG(1) << "RequirePin: " << device_path.value();
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  if (!SimTryPin(device_path.value(), pin)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, shill::kErrorResultIncorrectPin, ""));
    return;
  }
  SimLockStatus status = GetSimLockStatus(device_path.value());
  status.lock_enabled = require;
  SetSimLockStatus(device_path.value(), status);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::EnterPin(const dbus::ObjectPath& device_path,
                                     const std::string& pin,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  VLOG(1) << "EnterPin: " << device_path.value();
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  if (!SimTryPin(device_path.value(), pin)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, shill::kErrorResultIncorrectPin, ""));
    return;
  }
  SetSimLocked(device_path.value(), false);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::UnblockPin(const dbus::ObjectPath& device_path,
                                       const std::string& puk,
                                       const std::string& pin,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  VLOG(1) << "UnblockPin: " << device_path.value();
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  if (!SimTryPuk(device_path.value(), puk)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, shill::kErrorResultIncorrectPin, ""));
    return;
  }
  if (pin.length() < kSimPinMinLength) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback,
                                  shill::kErrorResultInvalidArguments, ""));
    return;
  }
  sim_pin_[device_path.value()] = pin;
  SetSimLocked(device_path.value(), false);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::ChangePin(const dbus::ObjectPath& device_path,
                                      const std::string& old_pin,
                                      const std::string& new_pin,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  VLOG(1) << "ChangePin: " << device_path.value();
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  if (!SimTryPin(device_path.value(), old_pin)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, shill::kErrorResultIncorrectPin, ""));
    return;
  }
  if (new_pin.length() < kSimPinMinLength) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback,
                                  shill::kErrorResultInvalidArguments, ""));
    return;
  }
  sim_pin_[device_path.value()] = new_pin;

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::Register(const dbus::ObjectPath& device_path,
                                     const std::string& network_id,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  base::Value* device_properties = stub_devices_.FindKey(device_path.value());
  if (!device_properties || !device_properties->is_dict()) {
    PostNotFoundError(error_callback);
    return;
  }
  base::Value* scan_results =
      device_properties->FindKey(shill::kFoundNetworksProperty);
  if (!scan_results) {
    PostError("No Cellular scan results", error_callback);
    return;
  }
  for (auto& network : scan_results->GetList()) {
    std::string id = network.FindKey(shill::kNetworkIdProperty)->GetString();
    std::string status = id == network_id ? "current" : "available";
    network.SetKey(shill::kStatusProperty, base::Value(status));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::SetCarrier(const dbus::ObjectPath& device_path,
                                       const std::string& carrier,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  SetPropertyInternal(device_path, shill::kCarrierProperty,
                      base::Value(carrier), callback, error_callback,
                      /*notify_changed=*/true);
}

void FakeShillDeviceClient::Reset(const dbus::ObjectPath& device_path,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::PerformTDLSOperation(
    const dbus::ObjectPath& device_path,
    const std::string& operation,
    const std::string& peer,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  // Use -1 to emulate a TDLS failure.
  if (tdls_busy_count_ == -1) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, shill::kErrorDhcpFailed, "Failed"));
    return;
  }
  if (operation != shill::kTDLSStatusOperation && tdls_busy_count_ > 0) {
    --tdls_busy_count_;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, shill::kErrorResultInProgress,
                                  "In-Progress"));
    return;
  }

  tdls_busy_count_ = initial_tdls_busy_count_;

  std::string result;
  if (operation == shill::kTDLSDiscoverOperation) {
    if (tdls_state_.empty())
      tdls_state_ = shill::kTDLSDisconnectedState;
  } else if (operation == shill::kTDLSSetupOperation) {
    if (tdls_state_.empty())
      tdls_state_ = shill::kTDLSConnectedState;
  } else if (operation == shill::kTDLSTeardownOperation) {
    if (tdls_state_.empty())
      tdls_state_ = shill::kTDLSDisconnectedState;
  } else if (operation == shill::kTDLSStatusOperation) {
    result = tdls_state_;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, result));
}

void FakeShillDeviceClient::AddWakeOnPacketConnection(
    const dbus::ObjectPath& device_path,
    const net::IPEndPoint& ip_endpoint,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }

  wake_on_packet_connections_[device_path].insert(ip_endpoint);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::AddWakeOnPacketOfTypes(
    const dbus::ObjectPath& device_path,
    const std::vector<std::string>& types,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }

  wake_on_packet_types_[device_path].insert(types.begin(), types.end());
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::RemoveWakeOnPacketConnection(
    const dbus::ObjectPath& device_path,
    const net::IPEndPoint& ip_endpoint,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  const auto device_iter = wake_on_packet_connections_.find(device_path);
  if (!stub_devices_.HasKey(device_path.value()) ||
      device_iter == wake_on_packet_connections_.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  const auto endpoint_iter = device_iter->second.find(ip_endpoint);
  if (endpoint_iter == device_iter->second.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  device_iter->second.erase(endpoint_iter);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::RemoveWakeOnPacketOfTypes(
    const dbus::ObjectPath& device_path,
    const std::vector<std::string>& types,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }

  const auto registered_types_iter = wake_on_packet_types_.find(device_path);
  if (registered_types_iter == wake_on_packet_types_.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  std::set<std::string>& registered_types = registered_types_iter->second;
  for (auto it = types.begin(); it != types.end(); it++)
    registered_types.erase(*it);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::RemoveAllWakeOnPacketConnections(
    const dbus::ObjectPath& device_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  const auto iter = wake_on_packet_connections_.find(device_path);
  if (!stub_devices_.HasKey(device_path.value()) ||
      iter == wake_on_packet_connections_.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  wake_on_packet_connections_.erase(iter);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

ShillDeviceClient::TestInterface* FakeShillDeviceClient::GetTestInterface() {
  return this;
}

// ShillDeviceClient::TestInterface overrides.

void FakeShillDeviceClient::AddDevice(const std::string& device_path,
                                      const std::string& type,
                                      const std::string& name) {
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->AddDevice(device_path);

  base::Value* properties = GetDeviceProperties(device_path);
  properties->SetKey(shill::kTypeProperty, base::Value(type));
  properties->SetKey(shill::kNameProperty, base::Value(name));
  properties->SetKey(shill::kDBusObjectProperty, base::Value(device_path));
  properties->SetKey(shill::kDBusServiceProperty,
                     base::Value(modemmanager::kModemManager1ServiceName));
  if (type == shill::kTypeCellular) {
    properties->SetKey(shill::kCellularAllowRoamingProperty,
                       base::Value(false));
  }
}

void FakeShillDeviceClient::RemoveDevice(const std::string& device_path) {
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->RemoveDevice(device_path);

  stub_devices_.RemoveWithoutPathExpansion(device_path, NULL);
}

void FakeShillDeviceClient::ClearDevices() {
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->ClearDevices();

  stub_devices_.Clear();
}

void FakeShillDeviceClient::SetDeviceProperty(const std::string& device_path,
                                              const std::string& name,
                                              const base::Value& value,
                                              bool notify_changed) {
  VLOG(1) << "SetDeviceProperty: " << device_path << ": " << name << " = "
          << value;
  SetPropertyInternal(dbus::ObjectPath(device_path), name, value,
                      base::DoNothing(),
                      base::Bind(&ErrorFunction, device_path), notify_changed);
}

std::string FakeShillDeviceClient::GetDevicePathForType(
    const std::string& type) {
  for (base::DictionaryValue::Iterator iter(stub_devices_); !iter.IsAtEnd();
       iter.Advance()) {
    const base::DictionaryValue* properties = NULL;
    if (!iter.value().GetAsDictionary(&properties))
      continue;
    std::string prop_type;
    if (!properties->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                   &prop_type) ||
        prop_type != type)
      continue;
    return iter.key();
  }
  return std::string();
}

void FakeShillDeviceClient::SetTDLSBusyCount(int count) {
  tdls_busy_count_ = std::max(count, -1);
}

void FakeShillDeviceClient::SetTDLSState(const std::string& state) {
  tdls_state_ = state;
}

void FakeShillDeviceClient::SetSimLocked(const std::string& device_path,
                                         bool locked) {
  SimLockStatus status = GetSimLockStatus(device_path);
  status.type = locked ? shill::kSIMLockPin : "";
  status.retries_left = kSimPinRetryCount;
  SetSimLockStatus(device_path, status);
}

void FakeShillDeviceClient::AddCellularFoundNetwork(
    const std::string& device_path) {
  base::Value* device_properties = stub_devices_.FindKey(device_path);
  if (!device_properties || !device_properties->is_dict()) {
    LOG(ERROR) << "Device path not found: " << device_path;
    return;
  }
  std::string type =
      device_properties->FindKey(shill::kTypeProperty)->GetString();
  if (type != shill::kTypeCellular) {
    LOG(ERROR) << "AddCellularNetwork called for non Cellular network: "
               << device_path;
    return;
  }

  // Add a new scan result entry
  base::Value* scan_results =
      device_properties->FindKey(shill::kFoundNetworksProperty);
  if (!scan_results) {
    scan_results = device_properties->SetKey(shill::kFoundNetworksProperty,
                                             base::ListValue());
  }
  base::DictionaryValue new_result;
  int idx = static_cast<int>(scan_results->GetList().size());
  new_result.SetKey(shill::kNetworkIdProperty,
                    base::Value(base::StringPrintf("network%d", idx)));
  new_result.SetKey(shill::kLongNameProperty,
                    base::Value(base::StringPrintf("Network %d", idx)));
  new_result.SetKey(shill::kTechnologyProperty, base::Value("GSM"));
  new_result.SetKey(shill::kStatusProperty, base::Value("available"));
  scan_results->GetList().push_back(std::move(new_result));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillDeviceClient::NotifyObserversPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     dbus::ObjectPath(device_path),
                     shill::kFoundNetworksProperty));
}

// Private Methods -------------------------------------------------------------

FakeShillDeviceClient::SimLockStatus FakeShillDeviceClient::GetSimLockStatus(
    const std::string& device_path) {
  SimLockStatus status;
  base::DictionaryValue* device_properties = nullptr;
  base::DictionaryValue* simlock_dict = nullptr;
  if (stub_devices_.GetDictionaryWithoutPathExpansion(device_path,
                                                      &device_properties) &&
      device_properties->GetDictionaryWithoutPathExpansion(
          shill::kSIMLockStatusProperty, &simlock_dict)) {
    simlock_dict->GetStringWithoutPathExpansion(shill::kSIMLockTypeProperty,
                                                &status.type);
    simlock_dict->GetIntegerWithoutPathExpansion(
        shill::kSIMLockRetriesLeftProperty, &status.retries_left);
    simlock_dict->GetBooleanWithoutPathExpansion(shill::kSIMLockEnabledProperty,
                                                 &status.lock_enabled);
    if (status.type == shill::kSIMLockPin && status.retries_left == 0)
      status.retries_left = kSimPinRetryCount;
  }
  return status;
}

void FakeShillDeviceClient::SetSimLockStatus(const std::string& device_path,
                                             const SimLockStatus& status) {
  base::Value* device_properties =
      stub_devices_.FindKeyOfType(device_path, base::Value::Type::DICTIONARY);

  if (!device_properties) {
    NOTREACHED() << "Device not found: " << device_path;
    return;
  }

  base::Value* simlock_dict =
      device_properties->SetKey(shill::kSIMLockStatusProperty,
                                base::Value(base::Value::Type::DICTIONARY));

  simlock_dict->SetKey(shill::kSIMLockTypeProperty, base::Value(status.type));
  simlock_dict->SetKey(shill::kSIMLockRetriesLeftProperty,
                       base::Value(status.retries_left));
  simlock_dict->SetKey(shill::kSIMLockEnabledProperty,
                       base::Value(status.lock_enabled));
  NotifyObserversPropertyChanged(dbus::ObjectPath(device_path),
                                 shill::kSIMLockStatusProperty);
}

bool FakeShillDeviceClient::SimTryPin(const std::string& device_path,
                                      const std::string& pin) {
  SimLockStatus status = GetSimLockStatus(device_path);
  if (status.type == shill::kSIMLockPuk) {
    VLOG(1) << "SimTryPin called with PUK locked.";
    return false;  // PUK locked, PIN won't work.
  }
  if (pin.length() < kSimPinMinLength)
    return false;
  std::string sim_pin = sim_pin_[device_path];
  if (sim_pin.empty()) {
    sim_pin = kDefaultSimPin;
    sim_pin_[device_path] = sim_pin;
  }
  if (pin == sim_pin) {
    status.type = "";
    status.retries_left = kSimPinRetryCount;
    SetSimLockStatus(device_path, status);
    return true;
  }

  VLOG(1) << "SIM PIN: " << pin << " != " << sim_pin
          << " Retries left: " << (status.retries_left - 1);
  if (--status.retries_left <= 0) {
    status.retries_left = kSimPukRetryCount;
    status.type = shill::kSIMLockPuk;
    status.lock_enabled = true;
  }
  SetSimLockStatus(device_path, status);
  return false;
}

bool FakeShillDeviceClient::SimTryPuk(const std::string& device_path,
                                      const std::string& puk) {
  SimLockStatus status = GetSimLockStatus(device_path);
  if (status.type != shill::kSIMLockPuk) {
    VLOG(1) << "PUK Not locked";
    return true;  // Not PUK locked.
  }
  if (status.retries_left == 0) {
    VLOG(1) << "PUK: No retries left";
    return false;  // Permanently locked.
  }

  if (puk == kSimPuk) {
    status.type = "";
    status.retries_left = kSimPinRetryCount;
    SetSimLockStatus(device_path, status);
    return true;
  }

  --status.retries_left;
  VLOG(1) << "SIM PUK: " << puk << " != " << kSimPuk
          << " Retries left: " << status.retries_left;
  SetSimLockStatus(device_path, status);
  return false;
}

void FakeShillDeviceClient::PassStubDeviceProperties(
    const dbus::ObjectPath& device_path,
    const DictionaryValueCallback& callback) const {
  const base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(device_path.value(),
                                                       &device_properties)) {
    base::DictionaryValue empty_dictionary;
    callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *device_properties);
}

// Posts a task to run a void callback with status code |status|.
void FakeShillDeviceClient::PostVoidCallback(VoidDBusMethodCallback callback,
                                             bool result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeShillDeviceClient::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& device_path,
    const std::string& property) {
  base::DictionaryValue* dict = NULL;
  std::string path = device_path.value();
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(path, &dict)) {
    LOG(ERROR) << "Notify for unknown device: " << path;
    return;
  }
  base::Value* value = NULL;
  if (!dict->GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: " << path << " : " << property;
    return;
  }
  for (auto& observer : GetObserverList(device_path))
    observer.OnPropertyChanged(property, *value);
}

base::Value* FakeShillDeviceClient::GetDeviceProperties(
    const std::string& device_path) {
  base::Value* properties =
      stub_devices_.FindKeyOfType(device_path, base::Value::Type::DICTIONARY);
  if (properties)
    return properties;
  return stub_devices_.SetKey(device_path,
                              base::Value(base::Value::Type::DICTIONARY));
}

FakeShillDeviceClient::PropertyObserverList&
FakeShillDeviceClient::GetObserverList(const dbus::ObjectPath& device_path) {
  auto iter = observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = base::WrapUnique(observer_list);
  return *observer_list;
}

}  // namespace chromeos
