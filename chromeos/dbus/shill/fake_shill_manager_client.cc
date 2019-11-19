// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_shill_manager_client.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/shill/fake_shill_device_client.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Allow parsed command line option 'tdls_busy' to set the fake busy count.
int s_tdls_busy_count = 0;
int s_extra_wifi_networks = 0;

// For testing dynamic WEP networks (uses wifi2).
bool s_dynamic_wep = false;

// Used to compare values for finding entries to erase in a ListValue.
// (ListValue only implements a const_iterator version of Find).
struct ValueEquals {
  explicit ValueEquals(const base::Value* first) : first_(first) {}
  bool operator()(const base::Value* second) const {
    return first_->Equals(second);
  }
  const base::Value* first_;
};

bool GetBoolValue(const base::Value& dict, const char* key) {
  const base::Value* value =
      dict.FindKeyOfType(key, base::Value::Type::BOOLEAN);
  return value ? value->GetBool() : false;
}

int GetIntValue(const base::Value& dict, const char* key) {
  const base::Value* value =
      dict.FindKeyOfType(key, base::Value::Type::INTEGER);
  return value ? value->GetInt() : 0;
}

std::string GetStringValue(const base::Value& dict, const char* key) {
  const base::Value* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  return value ? value->GetString() : std::string();
}

bool IsPortalledState(const std::string& state) {
  return state == shill::kStatePortal || state == shill::kStateNoConnectivity ||
         state == shill::kStateRedirectFound ||
         state == shill::kStatePortalSuspected;
}

int GetStateOrder(const base::Value& dict) {
  std::string state = GetStringValue(dict, shill::kStateProperty);
  if (state == shill::kStateOnline)
    return 1;
  if (state == shill::kStateReady)
    return 2;
  if (IsPortalledState(state))
    return 3;
  if (state == shill::kStateAssociation || state == shill::kStateConfiguration)
    return 4;
  return 5;
}

int GetTechnologyOrder(const base::Value& dict) {
  std::string technology = GetStringValue(dict, shill::kTypeProperty);
  // Note: In Shill, VPN is the highest priority network, but it is generally
  // dependent on the underlying network and gets sorted after that network.
  // For now, we simulate this by sorting VPN last. TODO(stevenjb): Support
  // VPN dependencies.
  if (technology == shill::kTypeVPN)
    return 10;

  if (technology == shill::kTypeEthernet)
    return 1;
  if (technology == shill::kTypeWifi)
    return 2;
  if (technology == shill::kTypeCellular)
    return 3;
  return 4;
}

int GetSecurityOrder(const base::Value& dict) {
  std::string security = GetStringValue(dict, shill::kSecurityProperty);
  // No security is listed last.
  if (security == shill::kSecurityNone)
    return 3;

  // 8021x is listed first.
  if (security == shill::kSecurity8021x)
    return 1;

  // All other security types are equal priority.
  return 2;
}

// Matches Shill's Service::Compare function.
bool CompareNetworks(const base::Value& a, const base::Value& b) {
  // Connection State: Online, Connected, Portal, Connecting
  int state_order_a = GetStateOrder(a);
  int state_order_b = GetStateOrder(b);
  if (state_order_a != state_order_b)
    return state_order_a < state_order_b;

  // Connectable (i.e. configured)
  bool connectable_a = GetBoolValue(a, shill::kConnectableProperty);
  bool connectable_b = GetBoolValue(b, shill::kConnectableProperty);
  if (connectable_a != connectable_b)
    return connectable_a;

  // Note: VPN is normally sorted first because of dependencies, see comment
  // in GetTechnologyOrder.

  // Technology
  int technology_order_a = GetTechnologyOrder(a);
  int technology_order_b = GetTechnologyOrder(b);
  if (technology_order_a != technology_order_b)
    return technology_order_a < technology_order_b;

  // Priority
  int priority_a = GetIntValue(a, shill::kPriorityProperty);
  int priority_b = GetIntValue(b, shill::kPriorityProperty);
  if (priority_a != priority_b)
    return priority_a > priority_b;

  // TODO: Sort on: Managed

  // AutoConnect
  bool auto_connect_a = GetBoolValue(a, shill::kAutoConnectProperty);
  bool auto_connect_b = GetBoolValue(b, shill::kAutoConnectProperty);
  if (auto_connect_a != auto_connect_b)
    return auto_connect_a;

  // Security
  int security_order_a = GetSecurityOrder(a);
  int security_order_b = GetSecurityOrder(b);
  if (security_order_a != security_order_b)
    return security_order_a < security_order_b;

  // TODO: Sort on: Profile: User profile < Device profile
  // TODO: Sort on: Has ever connected

  // SignalStrength
  int strength_a = GetIntValue(a, shill::kSignalStrengthProperty);
  int strength_b = GetIntValue(b, shill::kSignalStrengthProperty);
  if (strength_a != strength_b)
    return strength_a > strength_b;

  // Arbitrary identifier: SSID
  return GetStringValue(a, shill::kSSIDProperty) <
         GetStringValue(b, shill::kSSIDProperty);
}

void LogErrorCallback(const std::string& error_name,
                      const std::string& error_message) {
  LOG(ERROR) << error_name << ": " << error_message;
}

bool IsConnectedState(const std::string& state) {
  return state == shill::kStateOnline || IsPortalledState(state) ||
         state == shill::kStateReady;
}

void UpdatePortaledWifiState(const std::string& service_path) {
  ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
      service_path, shill::kStateProperty,
      base::Value(shill::kStateNoConnectivity));
}

bool IsCellularTechnology(const std::string& type) {
  return (type == shill::kNetworkTechnology1Xrtt ||
          type == shill::kNetworkTechnologyEvdo ||
          type == shill::kNetworkTechnologyGsm ||
          type == shill::kNetworkTechnologyGprs ||
          type == shill::kNetworkTechnologyEdge ||
          type == shill::kNetworkTechnologyUmts ||
          type == shill::kNetworkTechnologyHspa ||
          type == shill::kNetworkTechnologyHspaPlus ||
          type == shill::kNetworkTechnologyLte ||
          type == shill::kNetworkTechnologyLteAdvanced);
}

void SetInitialDeviceProperty(const std::string& device_path,
                              const std::string& name,
                              const base::Value& value) {
  ShillDeviceClient::Get()->GetTestInterface()->SetDeviceProperty(
      device_path, name, value, /*notify_changed=*/false);
}

const char kPathKey[] = "path";

const char kTechnologyUnavailable[] = "unavailable";
const char kTechnologyInitializing[] = "initializing";
const char kNetworkActivated[] = "activated";
const char kNetworkDisabled[] = "disabled";
const char kCellularServicePath[] = "/service/cellular1";
const char kRoamingRequired[] = "required";

}  // namespace

// static
const char FakeShillManagerClient::kFakeEthernetNetworkGuid[] = "eth1_guid";

FakeShillManagerClient::FakeShillManagerClient()
    : cellular_technology_(shill::kNetworkTechnologyGsm) {
  ParseCommandLineSwitch();
}

FakeShillManagerClient::~FakeShillManagerClient() = default;

// ShillManagerClient overrides.

void FakeShillManagerClient::AddPropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FakeShillManagerClient::RemovePropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeShillManagerClient::GetProperties(
    const DictionaryValueCallback& callback) {
  VLOG(1) << "Manager.GetProperties";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeShillManagerClient::PassStubProperties,
                                weak_ptr_factory_.GetWeakPtr(), callback));
}

void FakeShillManagerClient::GetNetworksForGeolocation(
    const DictionaryValueCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeShillManagerClient::PassStubGeoNetworks,
                                weak_ptr_factory_.GetWeakPtr(), callback));
}

void FakeShillManagerClient::SetProperty(const std::string& name,
                                         const base::Value& value,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  VLOG(2) << "SetProperty: " << name;
  stub_properties_.SetKey(name, value.Clone());
  CallNotifyObserversPropertyChanged(name);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillManagerClient::RequestScan(const std::string& type,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  VLOG(1) << "RequestScan: " << type;
  // For Stub purposes, default to a Wifi scan.
  std::string device_type = type.empty() ? shill::kTypeWifi : type;
  ShillDeviceClient::TestInterface* device_client =
      ShillDeviceClient::Get()->GetTestInterface();
  std::string device_path = device_client->GetDevicePathForType(device_type);
  if (!device_path.empty()) {
    device_client->SetDeviceProperty(device_path, shill::kScanningProperty,
                                     base::Value(true),
                                     /*notify_changed=*/true);
    if (device_type == shill::kTypeCellular)
      device_client->AddCellularFoundNetwork(device_path);
  }
  // Trigger |callback| immediately to indicate that the scan started.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::ScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), device_path),
      interactive_delay_);
}

void FakeShillManagerClient::EnableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ListValue* enabled_list = nullptr;
  if (!stub_properties_.GetListWithoutPathExpansion(
          shill::kAvailableTechnologiesProperty, &enabled_list)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, "StubError", "Property not found"));
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::SetTechnologyEnabled,
                     weak_ptr_factory_.GetWeakPtr(), type, callback, true),
      interactive_delay_);
}

void FakeShillManagerClient::DisableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ListValue* enabled_list = nullptr;
  if (!stub_properties_.GetListWithoutPathExpansion(
          shill::kAvailableTechnologiesProperty, &enabled_list)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback, "StubError", "Property not found"));
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::SetTechnologyEnabled,
                     weak_ptr_factory_.GetWeakPtr(), type, callback, false),
      interactive_delay_);
}

void FakeShillManagerClient::ConfigureService(
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  switch (simulate_configuration_result_) {
    case FakeShillSimulatedResult::kSuccess:
      break;
    case FakeShillSimulatedResult::kFailure:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(error_callback, "Error", "Simulated failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
  }

  ShillServiceClient::TestInterface* service_client =
      ShillServiceClient::Get()->GetTestInterface();

  std::string guid;
  std::string type;
  std::string name;
  if (!properties.GetString(shill::kGuidProperty, &guid) ||
      !properties.GetString(shill::kTypeProperty, &type)) {
    LOG(ERROR) << "ConfigureService requires GUID and Type to be defined";
    // If the properties aren't filled out completely, then just return an empty
    // object path.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, dbus::ObjectPath()));
    return;
  }

  if (type == shill::kTypeWifi) {
    properties.GetString(shill::kSSIDProperty, &name);

    if (name.empty()) {
      std::string hex_name;
      properties.GetString(shill::kWifiHexSsid, &hex_name);
      if (!hex_name.empty()) {
        std::vector<uint8_t> bytes;
        if (base::HexStringToBytes(hex_name, &bytes)) {
          name.assign(reinterpret_cast<const char*>(&bytes[0]), bytes.size());
        }
      }
    }
  }
  if (name.empty())
    properties.GetString(shill::kNameProperty, &name);
  if (name.empty())
    name = guid;

  std::string ipconfig_path;
  properties.GetString(shill::kIPConfigProperty, &ipconfig_path);

  std::string service_path = service_client->FindServiceMatchingGUID(guid);
  if (service_path.empty())
    service_path = service_client->FindSimilarService(properties);
  if (service_path.empty()) {
    // In the stub, service paths don't have to be DBus paths, so build
    // something out of the GUID as service path.
    // Don't use the GUID itself, so tests are forced to distinguish between
    // service paths and GUIDs instead of assuming that service path == GUID.
    service_path = "service_path_for_" + guid;
    service_client->AddServiceWithIPConfig(
        service_path, guid /* guid */, name /* name */, type, shill::kStateIdle,
        ipconfig_path, true /* visible */);
  }

  // Merge the new properties with existing properties.
  const base::DictionaryValue* existing_properties =
      service_client->GetServiceProperties(service_path);
  std::unique_ptr<base::DictionaryValue> merged_properties(
      existing_properties->DeepCopy());
  merged_properties->MergeDictionary(&properties);

  // Now set all the properties.
  for (base::DictionaryValue::Iterator iter(*merged_properties);
       !iter.IsAtEnd(); iter.Advance()) {
    service_client->SetServiceProperty(service_path, iter.key(), iter.value());
  }

  // If the Profile property is set, add it to ProfileClient.
  std::string profile_path;
  merged_properties->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                   &profile_path);
  if (!profile_path.empty()) {
    auto* profile_client = ShillProfileClient::Get()->GetTestInterface();
    if (!profile_client->UpdateService(profile_path, service_path))
      profile_client->AddService(profile_path, service_path);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, dbus::ObjectPath(service_path)));
}

void FakeShillManagerClient::ConfigureServiceForProfile(
    const dbus::ObjectPath& profile_path,
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
  std::string profile_property;
  properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                           &profile_property);
  CHECK(profile_property == profile_path.value());
  ConfigureService(properties, callback, error_callback);
}

void FakeShillManagerClient::GetService(const base::DictionaryValue& properties,
                                        const ObjectPathCallback& callback,
                                        const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, dbus::ObjectPath()));
}

void FakeShillManagerClient::ConnectToBestServices(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (best_service_.empty()) {
    VLOG(1) << "No 'best' service set.";
    return;
  }

  ShillServiceClient::Get()->Connect(dbus::ObjectPath(best_service_), callback,
                                     error_callback);
}

ShillManagerClient::TestInterface* FakeShillManagerClient::GetTestInterface() {
  return this;
}

// ShillManagerClient::TestInterface overrides.

void FakeShillManagerClient::AddDevice(const std::string& device_path) {
  if (GetListProperty(shill::kDevicesProperty)
          ->AppendIfNotPresent(std::make_unique<base::Value>(device_path))) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
  }
}

void FakeShillManagerClient::RemoveDevice(const std::string& device_path) {
  base::Value device_path_value(device_path);
  if (GetListProperty(shill::kDevicesProperty)
          ->Remove(device_path_value, nullptr)) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
  }
}

void FakeShillManagerClient::ClearDevices() {
  GetListProperty(shill::kDevicesProperty)->Clear();
  CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
}

void FakeShillManagerClient::AddTechnology(const std::string& type,
                                           bool enabled) {
  if (GetListProperty(shill::kAvailableTechnologiesProperty)
          ->AppendIfNotPresent(std::make_unique<base::Value>(type))) {
    CallNotifyObserversPropertyChanged(shill::kAvailableTechnologiesProperty);
  }
  if (enabled &&
      GetListProperty(shill::kEnabledTechnologiesProperty)
          ->AppendIfNotPresent(std::make_unique<base::Value>(type))) {
    CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
  }
}

void FakeShillManagerClient::RemoveTechnology(const std::string& type) {
  base::Value type_value(type);
  if (GetListProperty(shill::kAvailableTechnologiesProperty)
          ->Remove(type_value, nullptr)) {
    CallNotifyObserversPropertyChanged(shill::kAvailableTechnologiesProperty);
  }
  if (GetListProperty(shill::kEnabledTechnologiesProperty)
          ->Remove(type_value, nullptr)) {
    CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
  }
}

void FakeShillManagerClient::SetTechnologyInitializing(const std::string& type,
                                                       bool initializing) {
  if (initializing) {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)
            ->AppendIfNotPresent(std::make_unique<base::Value>(type))) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty);
    }
  } else {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)
            ->Remove(base::Value(type), nullptr)) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty);
    }
  }
}

void FakeShillManagerClient::AddGeoNetwork(
    const std::string& technology,
    const base::DictionaryValue& network) {
  base::Value* list_value =
      stub_geo_networks_.FindKeyOfType(technology, base::Value::Type::LIST);
  if (!list_value) {
    list_value = stub_geo_networks_.SetKey(
        technology, base::Value(base::Value::Type::LIST));
  }
  list_value->Append(network.Clone());
}

void FakeShillManagerClient::AddProfile(const std::string& profile_path) {
  const char* key = shill::kProfilesProperty;
  if (GetListProperty(key)->AppendIfNotPresent(
          std::make_unique<base::Value>(profile_path))) {
    CallNotifyObserversPropertyChanged(key);
  }
}

void FakeShillManagerClient::ClearProperties() {
  stub_properties_.Clear();
}

void FakeShillManagerClient::SetManagerProperty(const std::string& key,
                                                const base::Value& value) {
  SetProperty(key, value, base::DoNothing(), base::Bind(&LogErrorCallback));
}

void FakeShillManagerClient::AddManagerService(const std::string& service_path,
                                               bool notify_observers) {
  VLOG(2) << "AddManagerService: " << service_path;
  GetListProperty(shill::kServiceCompleteListProperty)
      ->AppendIfNotPresent(std::make_unique<base::Value>(service_path));
  SortManagerServices(false);
  if (notify_observers)
    CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::RemoveManagerService(
    const std::string& service_path) {
  VLOG(2) << "RemoveManagerService: " << service_path;
  base::Value service_path_value(service_path);
  GetListProperty(shill::kServiceCompleteListProperty)
      ->Remove(service_path_value, nullptr);
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::ClearManagerServices() {
  VLOG(1) << "ClearManagerServices";
  GetListProperty(shill::kServiceCompleteListProperty)->Clear();
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::ServiceStateChanged(
    const std::string& service_path,
    const std::string& state) {
  if (service_path == default_service_ && !IsConnectedState(state)) {
    // Default service is no longer connected; clear.
    default_service_.clear();
    base::Value default_service_value(default_service_);
    SetManagerProperty(shill::kDefaultServiceProperty, default_service_value);
  }
}

void FakeShillManagerClient::SortManagerServices(bool notify) {
  VLOG(1) << "SortManagerServices";

  // ServiceCompleteList contains string path values for each service.
  base::Value* complete_path_list = stub_properties_.FindKeyOfType(
      shill::kServiceCompleteListProperty, base::Value::Type::LIST);
  if (!complete_path_list || complete_path_list->GetList().empty())
    return;

  base::Value prev_complete_path_list = complete_path_list->Clone();

  // Networks for disabled services get appended to the end without sorting.
  std::vector<std::string> disabled_path_list;

  // Build a list of dictionaries for each service in the list.
  std::vector<base::Value> complete_dict_list;
  for (const base::Value& value : complete_path_list->GetList()) {
    std::string service_path = value.GetString();
    const base::Value* properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            service_path);
    if (!properties) {
      LOG(ERROR) << "Properties not found for service: " << service_path;
      continue;
    }

    std::string type = GetStringValue(*properties, shill::kTypeProperty);
    if (!TechnologyEnabled(type)) {
      disabled_path_list.push_back(service_path);
      continue;
    }

    base::Value properties_copy = properties->Clone();
    properties_copy.SetKey(kPathKey, base::Value(service_path));
    complete_dict_list.emplace_back(std::move(properties_copy));
  }

  if (complete_dict_list.empty())
    return;

  // Sort the service list using the same logic as Shill's Service::Compare.
  std::sort(complete_dict_list.begin(), complete_dict_list.end(),
            CompareNetworks);

  // Rebuild |complete_path_list| with the new sort order.
  complete_path_list->GetList().clear();
  for (const base::Value& dict : complete_dict_list) {
    std::string service_path = GetStringValue(dict, kPathKey);
    complete_path_list->Append(base::Value(service_path));
  }
  // Append disabled networks to the end of the complete path list.
  for (const std::string& path : disabled_path_list)
    complete_path_list->Append(base::Value(path));

  // Notify observers if the order changed.
  if (notify && *complete_path_list != prev_complete_path_list)
    CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);

  // Set the first connected service as the Default service. Note:
  // |new_default_service| may be empty indicating no default network.
  std::string new_default_service;
  const base::Value& default_network = complete_dict_list[0];
  if (IsConnectedState(
          GetStringValue(default_network, shill::kStateProperty))) {
    new_default_service = GetStringValue(default_network, kPathKey);
  }
  if (default_service_ != new_default_service) {
    default_service_ = new_default_service;
    SetManagerProperty(shill::kDefaultServiceProperty,
                       base::Value(default_service_));
  }
}

base::TimeDelta FakeShillManagerClient::GetInteractiveDelay() const {
  return interactive_delay_;
}

void FakeShillManagerClient::SetInteractiveDelay(base::TimeDelta delay) {
  interactive_delay_ = delay;
}

void FakeShillManagerClient::SetBestServiceToConnect(
    const std::string& service_path) {
  best_service_ = service_path;
}

const ShillManagerClient::NetworkThrottlingStatus&
FakeShillManagerClient::GetNetworkThrottlingStatus() {
  return network_throttling_status_;
}

void FakeShillManagerClient::SetNetworkThrottlingStatus(
    const NetworkThrottlingStatus& status,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  network_throttling_status_ = status;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

bool FakeShillManagerClient::GetFastTransitionStatus() {
  base::Value* fast_transition_status = stub_properties_.FindKey(
      base::StringPiece(shill::kWifiGlobalFTEnabledProperty));
  return fast_transition_status && fast_transition_status->GetBool();
}

void FakeShillManagerClient::SetSimulateConfigurationResult(
    FakeShillSimulatedResult configuration_result) {
  simulate_configuration_result_ = configuration_result;
}

void FakeShillManagerClient::SetupDefaultEnvironment() {
  // Bail out from setup if there is no message loop. This will be the common
  // case for tests that are not testing Shill.
  if (!base::ThreadTaskRunnerHandle::IsSet())
    return;

  ShillServiceClient::TestInterface* services =
      ShillServiceClient::Get()->GetTestInterface();
  DCHECK(services);
  ShillProfileClient::TestInterface* profiles =
      ShillProfileClient::Get()->GetTestInterface();
  DCHECK(profiles);
  ShillDeviceClient::TestInterface* devices =
      ShillDeviceClient::Get()->GetTestInterface();
  DCHECK(devices);
  ShillIPConfigClient::TestInterface* ip_configs =
      ShillIPConfigClient::Get()->GetTestInterface();
  DCHECK(ip_configs);

  const std::string shared_profile = ShillProfileClient::GetSharedProfilePath();
  profiles->AddProfile(shared_profile, std::string());

  const bool add_to_visible = true;

  // IPConfigs
  base::DictionaryValue ipconfig_v4_dictionary;
  ipconfig_v4_dictionary.SetKey(shill::kAddressProperty,
                                base::Value("100.0.0.1"));
  ipconfig_v4_dictionary.SetKey(shill::kGatewayProperty,
                                base::Value("100.0.0.2"));
  ipconfig_v4_dictionary.SetKey(shill::kPrefixlenProperty, base::Value(1));
  ipconfig_v4_dictionary.SetKey(shill::kMethodProperty,
                                base::Value(shill::kTypeIPv4));
  ipconfig_v4_dictionary.SetKey(shill::kWebProxyAutoDiscoveryUrlProperty,
                                base::Value("http://wpad.com/wpad.dat"));
  ip_configs->AddIPConfig("ipconfig_v4_path", ipconfig_v4_dictionary);
  base::DictionaryValue ipconfig_v6_dictionary;
  ipconfig_v6_dictionary.SetKey(shill::kAddressProperty,
                                base::Value("0:0:0:0:100:0:0:1"));
  ipconfig_v6_dictionary.SetKey(shill::kMethodProperty,
                                base::Value(shill::kTypeIPv6));
  ip_configs->AddIPConfig("ipconfig_v6_path", ipconfig_v6_dictionary);

  bool enabled;
  std::string state;

  // Ethernet
  state = GetInitialStateForType(shill::kTypeEthernet, &enabled);
  if (state == shill::kStateOnline || state == shill::kStateIdle) {
    AddTechnology(shill::kTypeEthernet, enabled);
    devices->AddDevice("/device/eth1", shill::kTypeEthernet,
                       "stub_eth_device1");
    SetInitialDeviceProperty("/device/eth1", shill::kAddressProperty,
                             base::Value("0123456789ab"));
    base::ListValue eth_ip_configs;
    eth_ip_configs.AppendString("ipconfig_v4_path");
    eth_ip_configs.AppendString("ipconfig_v6_path");
    SetInitialDeviceProperty("/device/eth1", shill::kIPConfigsProperty,
                             eth_ip_configs);
    const std::string kFakeEthernetNetworkPath = "/service/eth1";
    services->AddService(kFakeEthernetNetworkPath, kFakeEthernetNetworkGuid,
                         "eth1" /* name */, shill::kTypeEthernet, state,
                         add_to_visible);
    profiles->AddService(shared_profile, kFakeEthernetNetworkPath);
  }

  // Wifi
  if (s_tdls_busy_count != 0) {
    ShillDeviceClient::Get()->GetTestInterface()->SetTDLSBusyCount(
        s_tdls_busy_count);
  }

  state = GetInitialStateForType(shill::kTypeWifi, &enabled);
  if (state != kTechnologyUnavailable) {
    bool portaled = false;
    if (IsPortalledState(state)) {
      portaled = true;
      state = shill::kStateIdle;
    }
    AddTechnology(shill::kTypeWifi, enabled);
    devices->AddDevice("/device/wifi1", shill::kTypeWifi, "stub_wifi_device1");
    SetInitialDeviceProperty("/device/wifi1", shill::kAddressProperty,
                             base::Value("23456789abcd"));
    base::ListValue wifi_ip_configs;
    wifi_ip_configs.AppendString("ipconfig_v4_path");
    wifi_ip_configs.AppendString("ipconfig_v6_path");
    SetInitialDeviceProperty("/device/wifi1", shill::kIPConfigsProperty,
                             wifi_ip_configs);

    const std::string kWifi1Path = "/service/wifi1";
    services->AddService(kWifi1Path, "wifi1_guid", "wifi1" /* name */,
                         shill::kTypeWifi, state, add_to_visible);
    services->SetServiceProperty(kWifi1Path, shill::kSecurityClassProperty,
                                 base::Value(shill::kSecurityWep));
    services->SetServiceProperty(kWifi1Path, shill::kConnectableProperty,
                                 base::Value(true));
    profiles->AddService(shared_profile, kWifi1Path);

    const std::string kWifi2Path = "/service/wifi2";
    services->AddService(kWifi2Path, "wifi2_guid",
                         s_dynamic_wep ? "wifi2_WEP" : "wifi2_PSK" /* name */,
                         shill::kTypeWifi, shill::kStateIdle, add_to_visible);
    if (s_dynamic_wep) {
      services->SetServiceProperty(kWifi2Path, shill::kSecurityClassProperty,
                                   base::Value(shill::kSecurityWep));
      services->SetServiceProperty(kWifi2Path, shill::kEapKeyMgmtProperty,
                                   base::Value(shill::kKeyManagementIEEE8021X));
      services->SetServiceProperty(kWifi2Path, shill::kEapMethodProperty,
                                   base::Value(shill::kEapMethodPEAP));
      services->SetServiceProperty(kWifi2Path, shill::kEapIdentityProperty,
                                   base::Value("John Doe"));
    } else {
      services->SetServiceProperty(kWifi2Path, shill::kSecurityClassProperty,
                                   base::Value(shill::kSecurityPsk));
    }
    services->SetServiceProperty(kWifi2Path, shill::kConnectableProperty,
                                 base::Value(false));
    services->SetServiceProperty(kWifi2Path, shill::kPassphraseRequiredProperty,
                                 base::Value(true));
    services->SetServiceProperty(kWifi2Path, shill::kSignalStrengthProperty,
                                 base::Value(80));
    profiles->AddService(shared_profile, kWifi2Path);

    const std::string kWifi3Path = "/service/wifi3";
    services->AddService(kWifi3Path, "", /* empty GUID */
                         "wifi3" /* name */, shill::kTypeWifi,
                         shill::kStateIdle, add_to_visible);
    services->SetServiceProperty(kWifi3Path, shill::kSignalStrengthProperty,
                                 base::Value(40));

    if (portaled) {
      const std::string kPortaledWifiPath = "/service/portaled_wifi";
      services->AddService(kPortaledWifiPath, "portaled_wifi_guid",
                           "Portaled Wifi" /* name */, shill::kTypeWifi,
                           shill::kStateIdle, add_to_visible);
      services->SetServiceProperty(kPortaledWifiPath,
                                   shill::kSecurityClassProperty,
                                   base::Value(shill::kSecurityNone));
      services->SetConnectBehavior(
          kPortaledWifiPath,
          base::Bind(&UpdatePortaledWifiState, kPortaledWifiPath));
      services->SetServiceProperty(
          kPortaledWifiPath, shill::kConnectableProperty, base::Value(true));
      profiles->AddService(shared_profile, kPortaledWifiPath);
    }

    for (int i = 0; i < s_extra_wifi_networks; ++i) {
      int id = 4 + i;
      std::string path = base::StringPrintf("/service/wifi%d", id);
      std::string guid = base::StringPrintf("wifi%d_guid", id);
      std::string name = base::StringPrintf("wifi%d", id);
      services->AddService(path, guid, name, shill::kTypeWifi,
                           shill::kStateIdle, add_to_visible);
    }
  }

  // Cellular
  state = GetInitialStateForType(shill::kTypeCellular, &enabled);
  VLOG(1) << "Cellular state: " << state << " Enabled: " << enabled;
  if (state == kTechnologyInitializing) {
    SetTechnologyInitializing(shill::kTypeCellular, true);
  } else if (state != kTechnologyUnavailable) {
    bool activated = false;
    if (state == kNetworkActivated) {
      activated = true;
      state = shill::kStateOnline;
    }
    AddTechnology(shill::kTypeCellular, enabled);
    devices->AddDevice("/device/cellular1", shill::kTypeCellular,
                       "stub_cellular_device1");
    if (roaming_state_ == kRoamingRequired) {
      SetInitialDeviceProperty("/device/cellular1",
                               shill::kProviderRequiresRoamingProperty,
                               base::Value(true));
    }
    if (cellular_technology_ == shill::kNetworkTechnologyGsm) {
      SetInitialDeviceProperty("/device/cellular1",
                               shill::kSupportNetworkScanProperty,
                               base::Value(true));
      SetInitialDeviceProperty("/device/cellular1", shill::kSIMPresentProperty,
                               base::Value(true));
      devices->SetSimLocked("/device/cellular1", false);
    }

    if (state != shill::kStateIdle) {
      services->AddService(kCellularServicePath, "cellular1_guid",
                           "cellular1" /* name */, shill::kTypeCellular, state,
                           add_to_visible);
      base::Value technology_value(cellular_technology_);
      SetInitialDeviceProperty("/device/cellular1",
                               shill::kTechnologyFamilyProperty,
                               technology_value);
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kNetworkTechnologyProperty,
                                   technology_value);
      base::Value strength_value(50);
      services->SetServiceProperty(
          kCellularServicePath, shill::kSignalStrengthProperty, strength_value);

      if (activated) {
        services->SetServiceProperty(
            kCellularServicePath, shill::kActivationStateProperty,
            base::Value(shill::kActivationStateActivated));
        services->SetServiceProperty(kCellularServicePath,
                                     shill::kConnectableProperty,
                                     base::Value(true));
      } else {
        services->SetServiceProperty(
            kCellularServicePath, shill::kActivationStateProperty,
            base::Value(shill::kActivationStateNotActivated));
      }

      base::Value payment_portal(base::Value::Type::DICTIONARY);
      payment_portal.SetKey(shill::kPaymentPortalMethod, base::Value("POST"));
      payment_portal.SetKey(shill::kPaymentPortalPostData,
                            base::Value("iccid=123&imei=456&mdn=789"));
      payment_portal.SetKey(shill::kPaymentPortalURL,
                            base::Value(cellular_olp_));
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kPaymentPortalProperty,
                                   std::move(payment_portal));

      std::string shill_roaming_state;
      if (roaming_state_ == kRoamingRequired)
        shill_roaming_state = shill::kRoamingStateRoaming;
      else if (roaming_state_.empty())
        shill_roaming_state = shill::kRoamingStateHome;
      else  // |roaming_state_| is expected to be a valid Shill state.
        shill_roaming_state = roaming_state_;
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kRoamingStateProperty,
                                   base::Value(shill_roaming_state));

      base::DictionaryValue apn;
      apn.SetKey(shill::kApnProperty, base::Value("testapn"));
      apn.SetKey(shill::kApnNameProperty, base::Value("Test APN"));
      apn.SetKey(shill::kApnLocalizedNameProperty,
                 base::Value("Localized Test APN"));
      apn.SetKey(shill::kApnUsernameProperty, base::Value("User1"));
      apn.SetKey(shill::kApnPasswordProperty, base::Value("password"));
      apn.SetKey(shill::kApnAuthenticationProperty, base::Value("chap"));
      base::DictionaryValue apn2;
      apn2.SetKey(shill::kApnProperty, base::Value("testapn2"));
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kCellularApnProperty, apn);
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kCellularLastGoodApnProperty, apn);
      base::ListValue apn_list;
      apn_list.Append(apn.CreateDeepCopy());
      apn_list.Append(apn2.CreateDeepCopy());
      SetInitialDeviceProperty("/device/cellular1",
                               shill::kCellularApnListProperty, apn_list);

      profiles->AddService(shared_profile, kCellularServicePath);
    }
  }

  // VPN
  state = GetInitialStateForType(shill::kTypeVPN, &enabled);
  if (state != kTechnologyUnavailable) {
    // Set the "Provider" dictionary properties. Note: when setting these in
    // Shill, "Provider.Type", etc keys are used, but when reading the values
    // "Provider" . "Type", etc keys are used. Here we are setting the values
    // that will be read (by the UI, tests, etc).
    base::DictionaryValue provider_properties_openvpn;
    provider_properties_openvpn.SetString(shill::kTypeProperty,
                                          shill::kProviderOpenVpn);
    provider_properties_openvpn.SetString(shill::kHostProperty, "vpn_host");

    services->AddService("/service/vpn1", "vpn1_guid", "vpn1" /* name */,
                         shill::kTypeVPN, state, add_to_visible);
    services->SetServiceProperty("/service/vpn1", shill::kProviderProperty,
                                 provider_properties_openvpn);
    profiles->AddService(shared_profile, "/service/vpn1");

    base::DictionaryValue provider_properties_l2tp;
    provider_properties_l2tp.SetString(shill::kTypeProperty,
                                       shill::kProviderL2tpIpsec);
    provider_properties_l2tp.SetString(shill::kHostProperty, "vpn_host2");

    services->AddService("/service/vpn2", "vpn2_guid", "vpn2" /* name */,
                         shill::kTypeVPN, shill::kStateIdle, add_to_visible);
    services->SetServiceProperty("/service/vpn2", shill::kProviderProperty,
                                 provider_properties_l2tp);
  }

  // Additional device states
  for (DevicePropertyMap::iterator iter1 = shill_device_property_map_.begin();
       iter1 != shill_device_property_map_.end(); ++iter1) {
    std::string device_type = iter1->first;
    std::string device_path = devices->GetDevicePathForType(device_type);
    for (ShillPropertyMap::iterator iter2 = iter1->second.begin();
         iter2 != iter1->second.end(); ++iter2) {
      SetInitialDeviceProperty(device_path, iter2->first, *(iter2->second));
      delete iter2->second;
    }
  }

  SortManagerServices(true);
}

// Private methods

void FakeShillManagerClient::PassStubProperties(
    const DictionaryValueCallback& callback) const {
  std::unique_ptr<base::DictionaryValue> stub_properties(
      stub_properties_.DeepCopy());
  stub_properties->SetWithoutPathExpansion(
      shill::kServiceCompleteListProperty,
      GetEnabledServiceList(shill::kServiceCompleteListProperty));
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *stub_properties);
}

void FakeShillManagerClient::PassStubGeoNetworks(
    const DictionaryValueCallback& callback) const {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, stub_geo_networks_);
}

void FakeShillManagerClient::CallNotifyObserversPropertyChanged(
    const std::string& property) {
  // Avoid unnecessary delayed task if we have no observers (e.g. during
  // initial setup).
  if (!observer_list_.might_have_observers())
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::NotifyObserversPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(), property));
}

void FakeShillManagerClient::NotifyObserversPropertyChanged(
    const std::string& property) {
  VLOG(1) << "NotifyObserversPropertyChanged: " << property;
  base::Value* value = nullptr;
  if (!stub_properties_.GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: " << property;
    return;
  }
  if (property == shill::kServiceCompleteListProperty) {
    std::unique_ptr<base::ListValue> services(GetEnabledServiceList(property));
    for (auto& observer : observer_list_)
      observer.OnPropertyChanged(property, *(services.get()));
    return;
  }
  for (auto& observer : observer_list_)
    observer.OnPropertyChanged(property, *value);
}

base::ListValue* FakeShillManagerClient::GetListProperty(
    const std::string& property) {
  base::Value* list_property =
      stub_properties_.FindKeyOfType(property, base::Value::Type::LIST);
  if (!list_property) {
    list_property =
        stub_properties_.SetKey(property, base::Value(base::Value::Type::LIST));
  }
  return static_cast<base::ListValue*>(list_property);
}

bool FakeShillManagerClient::TechnologyEnabled(const std::string& type) const {
  if (type == shill::kTypeVPN)
    return true;  // VPN is always "enabled" since there is no associated device
  if (type == shill::kTypeEthernetEap)
    return true;
  bool enabled = false;
  const base::ListValue* technologies;
  if (stub_properties_.GetListWithoutPathExpansion(
          shill::kEnabledTechnologiesProperty, &technologies)) {
    base::Value type_value(type);
    if (technologies->Find(type_value) != technologies->end())
      enabled = true;
  }
  return enabled;
}

void FakeShillManagerClient::SetTechnologyEnabled(const std::string& type,
                                                  const base::Closure& callback,
                                                  bool enabled) {
  base::ListValue* enabled_list =
      GetListProperty(shill::kEnabledTechnologiesProperty);
  if (enabled)
    enabled_list->AppendIfNotPresent(std::make_unique<base::Value>(type));
  else
    enabled_list->Remove(base::Value(type), nullptr);
  CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
  // May affect available services.
  SortManagerServices(true);
}

std::unique_ptr<base::ListValue> FakeShillManagerClient::GetEnabledServiceList(
    const std::string& property) const {
  auto new_service_list = std::make_unique<base::ListValue>();
  const base::ListValue* service_list;
  if (stub_properties_.GetListWithoutPathExpansion(property, &service_list)) {
    ShillServiceClient::TestInterface* service_client =
        ShillServiceClient::Get()->GetTestInterface();
    for (base::ListValue::const_iterator iter = service_list->begin();
         iter != service_list->end(); ++iter) {
      std::string service_path;
      if (!iter->GetAsString(&service_path))
        continue;
      const base::DictionaryValue* properties =
          service_client->GetServiceProperties(service_path);
      if (!properties) {
        LOG(ERROR) << "Properties not found for service: " << service_path;
        continue;
      }
      std::string type;
      properties->GetString(shill::kTypeProperty, &type);
      if (TechnologyEnabled(type))
        new_service_list->Append(iter->CreateDeepCopy());
    }
  }
  return new_service_list;
}

void FakeShillManagerClient::ScanCompleted(const std::string& device_path) {
  VLOG(1) << "ScanCompleted: " << device_path;
  if (!device_path.empty()) {
    ShillDeviceClient::Get()->GetTestInterface()->SetDeviceProperty(
        device_path, shill::kScanningProperty, base::Value(false),
        /*notify_changed=*/true);
  }
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::ParseCommandLineSwitch() {
  // Default setup
  SetInitialNetworkState(shill::kTypeEthernet, shill::kStateOnline);
  SetInitialNetworkState(shill::kTypeWifi, shill::kStateOnline);
  SetInitialNetworkState(shill::kTypeCellular, shill::kStateIdle);
  SetInitialNetworkState(shill::kTypeVPN, shill::kStateIdle);

  // Parse additional options
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kShillStub))
    return;

  std::string option_str =
      command_line->GetSwitchValueASCII(switches::kShillStub);
  VLOG(1) << "Parsing command line:" << option_str;
  base::StringPairs string_pairs;
  base::SplitStringIntoKeyValuePairs(option_str, '=', ',', &string_pairs);
  for (base::StringPairs::iterator iter = string_pairs.begin();
       iter != string_pairs.end(); ++iter) {
    ParseOption((*iter).first, (*iter).second);
  }
}

bool FakeShillManagerClient::ParseOption(const std::string& arg0,
                                         const std::string& arg1) {
  VLOG(1) << "Parsing command line option: '" << arg0 << "=" << arg1 << "'";
  if ((arg0 == "clear" || arg0 == "reset") && arg1 == "1") {
    shill_initial_state_map_.clear();
    return true;
  } else if (arg0 == "interactive") {
    int seconds = 3;
    if (!arg1.empty())
      base::StringToInt(arg1, &seconds);
    interactive_delay_ = base::TimeDelta::FromSeconds(seconds);
    return true;
  } else if (arg0 == "sim_lock") {
    bool locked = (arg1 == "1");
    base::DictionaryValue* simlock_dict = new base::DictionaryValue;
    simlock_dict->SetBoolean(shill::kSIMLockEnabledProperty, true);
    std::string lock_type = locked ? shill::kSIMLockPin : "";
    simlock_dict->SetString(shill::kSIMLockTypeProperty, lock_type);
    if (locked) {
      simlock_dict->SetInteger(shill::kSIMLockRetriesLeftProperty,
                               FakeShillDeviceClient::kSimPinRetryCount);
    }
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kSIMPresentProperty] =
                                  new base::Value(true);
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kSIMLockStatusProperty] = simlock_dict;
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kTechnologyFamilyProperty] =
                                  new base::Value(shill::kNetworkTechnologyGsm);
    return true;
  } else if (arg0 == "sim_present") {
    bool present = (arg1 == "1");
    base::Value* sim_present = new base::Value(present);
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kSIMPresentProperty] = sim_present;
    if (!present)
      shill_initial_state_map_[shill::kTypeCellular] = kNetworkDisabled;
    return true;
  } else if (arg0 == "tdls_busy") {
    if (!arg1.empty())
      base::StringToInt(arg1, &s_tdls_busy_count);
    else
      s_tdls_busy_count = 1;
    return true;
  } else if (arg0 == "olp") {
    cellular_olp_ = arg1;
    return true;
  } else if (arg0 == "roaming") {
    // "home", "roaming", or "required"
    roaming_state_ = arg1;
    return true;
  } else if (arg0 == "dynamic_wep" && arg1 == "1") {
    s_dynamic_wep = true;
    return true;
  }
  return SetInitialNetworkState(arg0, arg1);
}

bool FakeShillManagerClient::SetInitialNetworkState(
    std::string type_arg,
    const std::string& state_arg) {
  int state_arg_as_int = -1;
  base::StringToInt(state_arg, &state_arg_as_int);

  std::string state;
  if (state_arg.empty() || state_arg == "1" || state_arg == "on" ||
      state_arg == "enabled" || state_arg == "connected" ||
      state_arg == "online" || state_arg == "inactive") {
    // Enabled and connected (default value)
    state = shill::kStateOnline;
  } else if (state_arg == "0" || state_arg == "off" ||
             state_arg == shill::kStateIdle) {
    // Technology enabled, services are created but are not connected.
    state = shill::kStateIdle;
  } else if (type_arg == shill::kTypeWifi && state_arg_as_int > 1) {
    // Enabled and connected, add extra wifi networks.
    state = shill::kStateOnline;
    s_extra_wifi_networks = state_arg_as_int - 1;
  } else if (state_arg == "disabled" || state_arg == "disconnect") {
    // Technology disabled but available, services created but not connected.
    state = kNetworkDisabled;
  } else if (state_arg == "none" || state_arg == "offline") {
    // Technology not available, do not create services.
    state = kTechnologyUnavailable;
  } else if (state_arg == "initializing") {
    // Technology available but not initialized.
    state = kTechnologyInitializing;
  } else if (state_arg == "portal") {
    // Technology is enabled, a service is connected and in Portal state.
    state = shill::kStateNoConnectivity;
  } else if (state_arg == "active" || state_arg == "activated") {
    // Technology is enabled, a service is connected and Activated.
    state = kNetworkActivated;
  } else if (type_arg == shill::kTypeCellular &&
             IsCellularTechnology(state_arg)) {
    state = shill::kStateOnline;
    cellular_technology_ = state_arg;
  } else if (type_arg == shill::kTypeCellular && state_arg == "LTEAdvanced") {
    // Special case, Shill name contains a ' '.
    state = shill::kStateOnline;
    cellular_technology_ = shill::kNetworkTechnologyLteAdvanced;
  } else {
    LOG(ERROR) << "Unrecognized initial state: " << type_arg << "="
               << state_arg;
    return false;
  }

  // Special cases
  if (type_arg == "wireless") {
    shill_initial_state_map_[shill::kTypeWifi] = state;
    shill_initial_state_map_[shill::kTypeCellular] = state;
    return true;
  }
  // Convenience synonyms.
  if (type_arg == "eth")
    type_arg = shill::kTypeEthernet;

  if (type_arg != shill::kTypeEthernet && type_arg != shill::kTypeWifi &&
      type_arg != shill::kTypeCellular && type_arg != shill::kTypeVPN) {
    LOG(WARNING) << "Unrecognized Shill network type: " << type_arg;
    return false;
  }

  // Disabled ethernet is the same as unavailable.
  if (type_arg == shill::kTypeEthernet && state == kNetworkDisabled)
    state = kTechnologyUnavailable;

  shill_initial_state_map_[type_arg] = state;
  return true;
}

std::string FakeShillManagerClient::GetInitialStateForType(
    const std::string& type,
    bool* enabled) {
  std::string result;
  std::map<std::string, std::string>::const_iterator iter =
      shill_initial_state_map_.find(type);
  if (iter == shill_initial_state_map_.end()) {
    *enabled = false;
    result = kTechnologyUnavailable;
  } else {
    std::string state = iter->second;
    if (state == kNetworkDisabled) {
      *enabled = false;
      result = shill::kStateIdle;
    } else {
      *enabled = true;
      result = state;
    }
    if ((IsPortalledState(state) && type != shill::kTypeWifi) ||
        (state == kNetworkActivated && type != shill::kTypeCellular)) {
      LOG(WARNING) << "Invalid state: " << state << " for " << type;
      result = shill::kStateIdle;
    }
  }
  VLOG(1) << "Initial state for: " << type << " = " << result
          << " Enabled: " << *enabled;
  return result;
}

}  // namespace chromeos
