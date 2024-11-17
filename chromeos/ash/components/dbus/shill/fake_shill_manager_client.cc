// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Used to compare values for finding entries to erase in a ListValue.
// (ListValue only implements a const_iterator version of Find).
struct ValueEquals {
  explicit ValueEquals(const base::Value* first) : first_(first) {}
  bool operator()(const base::Value* second) const {
    return *first_ == *second;
  }
  raw_ptr<const base::Value> first_;
};

bool GetBoolValue(const base::Value::Dict& dict, const char* key) {
  return dict.FindBool(key).value_or(false);
}

int GetIntValue(const base::Value::Dict& dict, const char* key) {
  return dict.FindInt(key).value_or(0);
}

bool GetString(const base::Value::Dict& dict,
               const char* path,
               std::string* result) {
  const std::string* str_result = dict.FindStringByDottedPath(path);
  if (!str_result) {
    return false;
  }
  *result = *str_result;
  return true;
}

std::string GetStringValue(const base::Value::Dict& dict, const char* key) {
  const std::string* str_result = dict.FindString(key);
  return str_result ? *str_result : std::string();
}

// Returns whether added.
bool AppendIfNotPresent(base::Value::List& list, base::Value value) {
  if (base::Contains(list, value)) {
    return false;
  }
  list.Append(std::move(value));
  return true;
}

bool IsPortalledState(const std::string& state) {
  return state == shill::kStateNoConnectivity ||
         state == shill::kStateRedirectFound ||
         state == shill::kStatePortalSuspected;
}

int GetStateOrder(const base::Value::Dict& dict) {
  std::string state = GetStringValue(dict, shill::kStateProperty);
  if (state == shill::kStateOnline) {
    return 1;
  }
  if (state == shill::kStateReady) {
    return 2;
  }
  if (IsPortalledState(state)) {
    return 3;
  }
  if (state == shill::kStateAssociation ||
      state == shill::kStateConfiguration) {
    return 4;
  }
  return 5;
}

int GetTechnologyOrder(const base::Value::Dict& dict) {
  std::string technology = GetStringValue(dict, shill::kTypeProperty);
  // Note: In Shill, VPN is the highest priority network, but it is generally
  // dependent on the underlying network and gets sorted after that network.
  // For now, we simulate this by sorting VPN last. TODO(stevenjb): Support
  // VPN dependencies.
  if (technology == shill::kTypeVPN) {
    return 10;
  }

  if (technology == shill::kTypeEthernet) {
    return 1;
  }
  if (technology == shill::kTypeWifi) {
    return 2;
  }
  if (technology == shill::kTypeCellular) {
    return 3;
  }
  return 4;
}

int GetSecurityOrder(const base::Value::Dict& dict) {
  std::string security = GetStringValue(dict, shill::kSecurityClassProperty);
  // No security is listed last.
  if (security == shill::kSecurityClassNone) {
    return 3;
  }

  // 8021x is listed first.
  if (security == shill::kSecurityClass8021x) {
    return 1;
  }

  // All other security types are equal priority.
  return 2;
}

// Matches Shill's Service::Compare function.
bool CompareNetworks(const base::Value& a, const base::Value& b) {
  // Connection State: Online, Connected, Portal, Connecting
  const base::Value::Dict& a_dict = a.GetDict();
  const base::Value::Dict& b_dict = b.GetDict();
  int state_order_a = GetStateOrder(a_dict);
  int state_order_b = GetStateOrder(b_dict);
  if (state_order_a != state_order_b) {
    return state_order_a < state_order_b;
  }

  // Connectable (i.e. configured)
  bool connectable_a = GetBoolValue(a_dict, shill::kConnectableProperty);
  bool connectable_b = GetBoolValue(b_dict, shill::kConnectableProperty);
  if (connectable_a != connectable_b) {
    return connectable_a;
  }

  // Note: VPN is normally sorted first because of dependencies, see comment
  // in GetTechnologyOrder.

  // Technology
  int technology_order_a = GetTechnologyOrder(a_dict);
  int technology_order_b = GetTechnologyOrder(b_dict);
  if (technology_order_a != technology_order_b) {
    return technology_order_a < technology_order_b;
  }

  // Priority
  int priority_a = GetIntValue(a_dict, shill::kPriorityProperty);
  int priority_b = GetIntValue(b_dict, shill::kPriorityProperty);
  if (priority_a != priority_b) {
    return priority_a > priority_b;
  }

  // TODO: Sort on: Managed

  // AutoConnect
  bool auto_connect_a = GetBoolValue(a_dict, shill::kAutoConnectProperty);
  bool auto_connect_b = GetBoolValue(b_dict, shill::kAutoConnectProperty);
  if (auto_connect_a != auto_connect_b) {
    return auto_connect_a;
  }

  // Security
  int security_order_a = GetSecurityOrder(a_dict);
  int security_order_b = GetSecurityOrder(b_dict);
  if (security_order_a != security_order_b) {
    return security_order_a < security_order_b;
  }

  // TODO: Sort on: Profile: User profile < Device profile
  // TODO: Sort on: Has ever connected

  // SignalStrength
  int strength_a = GetIntValue(a_dict, shill::kSignalStrengthProperty);
  int strength_b = GetIntValue(b_dict, shill::kSignalStrengthProperty);
  if (strength_a != strength_b) {
    return strength_a > strength_b;
  }

  // Arbitrary identifier: SSID
  return GetStringValue(a_dict, shill::kSSIDProperty) <
         GetStringValue(b_dict, shill::kSSIDProperty);
}

void LogErrorCallback(const std::string& error_name,
                      const std::string& error_message) {
  LOG(ERROR) << error_name << ": " << error_message;
}

bool IsConnectedState(const std::string& state) {
  return state == shill::kStateOnline || IsPortalledState(state) ||
         state == shill::kStateReady;
}

void UpdatePortaledState(const std::string& service_path,
                         const std::string& state) {
  ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
      service_path, shill::kStateProperty, base::Value(state));
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
          type == shill::kNetworkTechnologyLteAdvanced ||
          type == shill::kNetworkTechnology5gNr);
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
const char kDefaultTetheringSSID[] = "test_tethering_ssid";
const char kDefaultTetheringPassphrase[] = "tetheringpassword";

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
    chromeos::DBusMethodCallback<base::Value::Dict> callback) {
  VLOG(1) << "Manager.GetProperties";
  if (return_null_properties_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeShillManagerClient::PassNullopt,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::PassStubProperties,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeShillManagerClient::GetNetworksForGeolocation(
    chromeos::DBusMethodCallback<base::Value::Dict> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::PassStubGeoNetworks,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeShillManagerClient::SetProperty(const std::string& name,
                                         const base::Value& value,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) {
  VLOG(2) << "SetProperty: " << name;
  stub_properties_.Set(name, value.Clone());
  CallNotifyObserversPropertyChanged(name);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillManagerClient::RequestScan(const std::string& type,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) {
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
    if (device_type == shill::kTypeCellular) {
      device_client->AddCellularFoundNetwork(device_path);
    }
  }
  // Trigger |callback| immediately to indicate that the scan started.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::ScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), device_path),
      interactive_delay_);
}

void FakeShillManagerClient::EnableTechnology(const std::string& type,
                                              base::OnceClosure callback,
                                              ErrorCallback error_callback) {
  base::Value::List* enabled_list =
      stub_properties_.FindList(shill::kAvailableTechnologiesProperty);
  if (!enabled_list) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback), "StubError",
                                  "Property not found"));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::SetTechnologyEnabled,
                     weak_ptr_factory_.GetWeakPtr(), type, std::move(callback),
                     true),
      interactive_delay_);
}

void FakeShillManagerClient::DisableTechnology(const std::string& type,
                                               base::OnceClosure callback,
                                               ErrorCallback error_callback) {
  base::Value::List* enabled_list =
      stub_properties_.FindList(shill::kAvailableTechnologiesProperty);
  if (!enabled_list) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback), "StubError",
                                  "Property not found"));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::SetTechnologyEnabled,
                     weak_ptr_factory_.GetWeakPtr(), type, std::move(callback),
                     false),
      interactive_delay_);
}

void FakeShillManagerClient::ConfigureService(
    const base::Value::Dict& properties,
    chromeos::ObjectPathCallback callback,
    ErrorCallback error_callback) {
  switch (simulate_configuration_result_) {
    case FakeShillSimulatedResult::kSuccess:
      break;
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(error_callback),
                                    simulate_configuration_error_.name,
                                    simulate_configuration_error_.message));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }

  ShillServiceClient::TestInterface* service_client =
      ShillServiceClient::Get()->GetTestInterface();

  std::string guid;
  std::string type;
  std::string name;
  if (!GetString(properties, shill::kGuidProperty, &guid) ||
      !GetString(properties, shill::kTypeProperty, &type)) {
    LOG(ERROR) << "ConfigureService requires GUID and Type to be defined";
    // If the properties aren't filled out completely, then just return an empty
    // object path.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), dbus::ObjectPath()));
    return;
  }

  if (type == shill::kTypeWifi) {
    GetString(properties, shill::kSSIDProperty, &name);

    // Use the hex SSID if the SSID was not found in the UI data.
    if (name.empty()) {
      std::string hex_name = GetStringValue(properties, shill::kWifiHexSsid);
      if (!hex_name.empty()) {
        std::vector<uint8_t> bytes;
        if (base::HexStringToBytes(hex_name, &bytes)) {
          name.assign(reinterpret_cast<const char*>(bytes.data()),
                      bytes.size());
        }
      }
    }
  }
  if (name.empty()) {
    GetString(properties, shill::kNameProperty, &name);
  }
  if (name.empty()) {
    name = guid;
  }

  std::string ipconfig_path;
  GetString(properties, shill::kIPConfigProperty, &ipconfig_path);

  std::string service_path = service_client->FindServiceMatchingGUID(guid);
  if (service_path.empty()) {
    service_path = service_client->FindSimilarService(properties);
  }
  if (service_path.empty()) {
    // shill specifies that non-wifi services are always visible.
    // For wifi services, let the test case decide.
    bool initial_visible =
        type != shill::kTypeWifi || wifi_services_visible_by_default_;
    // In the stub, service paths don't have to be DBus paths, so build
    // something out of the GUID as service path.
    // Don't use the GUID itself, so tests are forced to distinguish between
    // service paths and GUIDs instead of assuming that service path == GUID.
    service_path = "service_path_for_" + guid;
    service_client->AddServiceWithIPConfig(
        service_path, guid /* guid */, name /* name */, type, shill::kStateIdle,
        ipconfig_path, initial_visible);
  }

  // Set all the properties.
  for (auto iter : properties) {
    service_client->SetServiceProperty(service_path, iter.first, iter.second);
  }

  if (type == shill::kTypeWifi) {
    // Wi-Fi networks should always have an SSID. Since the `name` is determined
    // using the SSID or the hex SSID we can simply set the SSID property
    // directly and know that it will be set correctly if it wasn't already.
    // This is done to emulate Shill behavior.
    CHECK(!name.empty());
    service_client->SetServiceProperty(service_path, shill::kSSIDProperty,
                                       base::Value(name));
  }

  // If the Profile property is set, add it to ProfileClient.
  const std::string* profile_path =
      properties.FindString(shill::kProfileProperty);
  if (profile_path) {
    auto* profile_client = ShillProfileClient::Get()->GetTestInterface();
    if (!profile_client->UpdateService(*profile_path, service_path)) {
      profile_client->AddService(*profile_path, service_path);
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), dbus::ObjectPath(service_path)));
}

void FakeShillManagerClient::ConfigureServiceForProfile(
    const dbus::ObjectPath& profile_path,
    const base::Value::Dict& properties,
    chromeos::ObjectPathCallback callback,
    ErrorCallback error_callback) {
  base::Value::Dict properties_copy = properties.Clone();
  properties_copy.Set(shill::kProfileProperty, profile_path.value());
  ConfigureService(properties_copy, std::move(callback),
                   std::move(error_callback));
}

void FakeShillManagerClient::GetService(const base::Value::Dict& properties,
                                        chromeos::ObjectPathCallback callback,
                                        ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dbus::ObjectPath()));
}

void FakeShillManagerClient::ScanAndConnectToBestServices(
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  connect_to_best_services_callbacks_ =
      std::make_tuple(std::move(callback), std::move(error_callback));
  RequestScan(shill::kTypeWifi, base::DoNothing(),
              base::BindOnce(&LogErrorCallback));
}

void FakeShillManagerClient::ContinueConnectToBestServices(
    ConnectToBestServicesCallbacks connect_to_best_services_callbacks) {
  if (best_service_.empty()) {
    VLOG(1) << "No 'best' service set.";
    return;
  }

  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(best_service_),
      std::move(std::get<0>(connect_to_best_services_callbacks)),
      std::move(std::get<1>(connect_to_best_services_callbacks)));
}

void FakeShillManagerClient::AddPasspointCredentials(
    const dbus::ObjectPath& profile_path,
    const base::Value::Dict& properties,
    base::OnceClosure callback,
    ErrorCallback error_callback) {}

void FakeShillManagerClient::RemovePasspointCredentials(
    const dbus::ObjectPath& profile_path,
    const base::Value::Dict& properties,
    base::OnceClosure callback,
    ErrorCallback error_callback) {}

void FakeShillManagerClient::SetTetheringEnabled(bool enabled,
                                                 StringCallback callback,
                                                 ErrorCallback error_callback) {
  switch (simulate_tethering_enable_result_) {
    case FakeShillSimulatedResult::kSuccess: {
      // Set additional Hotspot properties when the result is a success.
      if (simulate_enable_tethering_result_string_ ==
          shill::kTetheringEnableResultSuccess) {
        base::Value::Dict tethering_state;
        if (enabled) {
          tethering_state.Set(shill::kTetheringStatusStateProperty,
                              shill::kTetheringStateActive);
          tethering_state.Set(shill::kTetheringStatusClientsProperty,
                              base::Value::List());
        } else {
          tethering_state.Set(shill::kTetheringStatusStateProperty,
                              shill::kTetheringStateIdle);
          tethering_state.Set(shill::kTetheringStatusIdleReasonProperty,
                              shill::kTetheringIdleReasonUserExit);
        }
        SetManagerProperty(shill::kTetheringStatusProperty,
                           base::Value(std::move(tethering_state)));
      }
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    simulate_enable_tethering_result_string_));
      return;
    }
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(error_callback), "Error",
                                    "Simulated failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress: {
      base::Value::Dict tethering_state;
      tethering_state.Set(shill::kTetheringStatusStateProperty,
                          shill::kTetheringStateStarting);
      SetManagerProperty(shill::kTetheringStatusProperty,
                         base::Value(std::move(tethering_state)));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), "In progress"));
      return;
    }
  }
}

void FakeShillManagerClient::EnableTethering(
    const shill::WiFiInterfacePriority& priority,
    StringCallback callback,
    ErrorCallback error_callback) {
  SetTetheringEnabled(/*enabled=*/true, std::move(callback),
                      std::move(error_callback));
}

void FakeShillManagerClient::DisableTethering(StringCallback callback,
                                              ErrorCallback error_callback) {
  SetTetheringEnabled(/*enabled=*/false, std::move(callback),
                      std::move(error_callback));
}

void FakeShillManagerClient::CheckTetheringReadiness(
    StringCallback callback,
    ErrorCallback error_callback) {
  switch (simulate_check_tethering_readiness_result_) {
    case FakeShillSimulatedResult::kSuccess:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    simulate_tethering_readiness_status_));
      return;
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(error_callback), "Error",
                                    "Simulated failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }
}

void FakeShillManagerClient::SetLOHSEnabled(bool enabled,
                                            base::OnceClosure callback,
                                            ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(error_callback), "Error", "Fake failure"));
  return;
}

void FakeShillManagerClient::CreateP2PGroup(
    const CreateP2PGroupParameter& create_group_argument,
    base::OnceCallback<void(base::Value::Dict result)> callback,
    ErrorCallback error_callback) {
  switch (simulate_create_p2p_group_result_) {
    case FakeShillSimulatedResult::kSuccess: {
      auto fake_success_result = base::Value::Dict().Set(
          shill::kP2PResultCode, simulate_create_p2p_group_result_code_);
      if (simulate_create_p2p_group_result_code_ !=
          shill::kCreateP2PGroupResultSuccess) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      std::move(fake_success_result)));
        return;
      }
      const int shill_id = 0;
      fake_success_result.Set(shill::kP2PDeviceShillID, shill_id);
      auto group_owner_info = base::Value::List().Append(
          base::Value::Dict()
              .Set(shill::kP2PGroupInfoShillIDProperty, shill_id)
              .Set(shill::kP2PGroupInfoInterfaceProperty, "p2p-wlan-0")
              .Set(shill::kP2PGroupInfoStateProperty,
                   shill::kP2PGroupInfoStateActive)
              .Set(shill::kP2PGroupInfoSSIDProperty,
                   create_group_argument.ssid ? *create_group_argument.ssid
                                              : "DIRECT-A0")
              .Set(shill::kP2PGroupInfoPassphraseProperty,
                   create_group_argument.passphrase
                       ? *create_group_argument.passphrase
                       : "direct-passphrase")
              .Set(shill::kP2PGroupInfoFrequencyProperty, 1000)
              .Set(shill::kP2PGroupInfoNetworkIDProperty, 1)
              .Set(shill::kP2PGroupInfoIPv4AddressProperty, "100.0.0.1"));
      SetManagerProperty(shill::kP2PGroupInfosProperty,
                         base::Value(std::move(group_owner_info)));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(fake_success_result)));
      return;
    }
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(error_callback), "Error", "Fake failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }
}

void FakeShillManagerClient::ConnectToP2PGroup(
    const ConnectP2PGroupParameter& connect_group_argument,
    base::OnceCallback<void(base::Value::Dict result)> callback,
    ErrorCallback error_callback) {
  switch (simulate_connect_p2p_group_result_) {
    case FakeShillSimulatedResult::kSuccess: {
      auto fake_success_result = base::Value::Dict().Set(
          shill::kP2PResultCode, simulate_connect_p2p_group_result_code_);
      if (simulate_connect_p2p_group_result_code_ !=
          shill::kConnectToP2PGroupResultSuccess) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      std::move(fake_success_result)));
        return;
      }
      const int shill_id = 0;
      fake_success_result.Set(shill::kP2PDeviceShillID, shill_id);
      auto group_owner_info = base::Value::List().Append(
          base::Value::Dict()
              .Set(shill::kP2PClientInfoShillIDProperty, shill_id)
              .Set(shill::kP2PClientInfoInterfaceProperty, "p2p-wlan-0")
              .Set(shill::kP2PClientInfoStateProperty,
                   shill::kP2PClientInfoStateConnected)
              .Set(shill::kP2PClientInfoSSIDProperty,
                   connect_group_argument.ssid)
              .Set(shill::kP2PClientInfoPassphraseProperty,
                   connect_group_argument.passphrase)
              .Set(shill::kP2PClientInfoFrequencyProperty,
                   connect_group_argument.frequency
                       ? static_cast<int>(*connect_group_argument.frequency)
                       : 1000)
              .Set(shill::kP2PClientInfoNetworkIDProperty, 1)
              .Set(shill::kP2PClientInfoIPv4AddressProperty, "100.0.0.1"));
      SetManagerProperty(shill::kP2PClientInfosProperty,
                         base::Value(std::move(group_owner_info)));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(fake_success_result)));
      return;
    }
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(error_callback), "Error", "Fake failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }
}

void FakeShillManagerClient::DestroyP2PGroup(
    const int shill_id,
    base::OnceCallback<void(base::Value::Dict result)> callback,
    ErrorCallback error_callback) {
  recent_destroyed_group_id = shill_id;
  switch (simulate_destroy_p2p_group_result_) {
    case FakeShillSimulatedResult::kSuccess: {
      auto fake_success_result = base::Value::Dict().Set(
          shill::kP2PResultCode, simulate_destroy_p2p_group_result_code_);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(fake_success_result)));
      return;
    }
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(error_callback), "Error", "Fake failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }
}

int FakeShillManagerClient::GetRecentlyDestroyedP2PGroupId() {
  return recent_destroyed_group_id;
}

void FakeShillManagerClient::DisconnectFromP2PGroup(
    const int shill_id,
    base::OnceCallback<void(base::Value::Dict result)> callback,
    ErrorCallback error_callback) {
  recent_disconnected_group_id = shill_id;
  switch (simulate_disconnect_p2p_group_result_) {
    case FakeShillSimulatedResult::kSuccess: {
      auto fake_success_result = base::Value::Dict().Set(
          shill::kP2PResultCode, simulate_disconnect_p2p_group_result_code_);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(fake_success_result)));
      return;
    }
    case FakeShillSimulatedResult::kFailure:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(error_callback), "Error", "Fake failure"));
      return;
    case FakeShillSimulatedResult::kTimeout:
      // No callbacks get executed and the caller should eventually timeout.
      return;
    case FakeShillSimulatedResult::kInProgress:
      // No callbacks get executed in this case.
      return;
  }
}

int FakeShillManagerClient::GetRecentlyDisconnectedP2PGroupId() {
  return recent_disconnected_group_id;
}

ShillManagerClient::TestInterface* FakeShillManagerClient::GetTestInterface() {
  return this;
}

// ShillManagerClient::TestInterface overrides.

void FakeShillManagerClient::AddDevice(const std::string& device_path) {
  if (AppendIfNotPresent(GetListProperty(shill::kDevicesProperty),
                         base::Value(device_path))) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
  }
}

void FakeShillManagerClient::RemoveDevice(const std::string& device_path) {
  base::Value device_path_value(device_path);
  if (GetListProperty(shill::kDevicesProperty).EraseValue(device_path_value)) {
    CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
  }
}

void FakeShillManagerClient::ClearDevices() {
  GetListProperty(shill::kDevicesProperty).clear();
  CallNotifyObserversPropertyChanged(shill::kDevicesProperty);
}

void FakeShillManagerClient::AddTechnology(const std::string& type,
                                           bool enabled) {
  if (AppendIfNotPresent(GetListProperty(shill::kAvailableTechnologiesProperty),
                         base::Value(type))) {
    CallNotifyObserversPropertyChanged(shill::kAvailableTechnologiesProperty);
  }
  if (enabled &&
      AppendIfNotPresent(GetListProperty(shill::kEnabledTechnologiesProperty),
                         base::Value(type))) {
    CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
  }
}

void FakeShillManagerClient::RemoveTechnology(const std::string& type) {
  base::Value type_value(type);
  if (GetListProperty(shill::kAvailableTechnologiesProperty)
          .EraseValue(type_value)) {
    CallNotifyObserversPropertyChanged(shill::kAvailableTechnologiesProperty);
  }
  if (GetListProperty(shill::kEnabledTechnologiesProperty)
          .EraseValue(type_value)) {
    CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
  }
}

void FakeShillManagerClient::SetTechnologyInitializing(const std::string& type,
                                                       bool initializing) {
  if (initializing) {
    if (AppendIfNotPresent(
            GetListProperty(shill::kUninitializedTechnologiesProperty),
            base::Value(type))) {
      if (GetListProperty(shill::kEnabledTechnologiesProperty)
              .EraseValue(base::Value(type))) {
        CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
      }

      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty);
    }
  } else {
    if (GetListProperty(shill::kUninitializedTechnologiesProperty)
            .EraseValue(base::Value(type))) {
      CallNotifyObserversPropertyChanged(
          shill::kUninitializedTechnologiesProperty);
    }
  }
}

void FakeShillManagerClient::SetTechnologyProhibited(const std::string& type,
                                                     bool prohibited) {
  std::string prohibited_technologies =
      GetStringValue(stub_properties_, shill::kProhibitedTechnologiesProperty);
  std::vector<std::string> prohibited_list =
      base::SplitString(prohibited_technologies, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  std::set<std::string> prohibited_set(prohibited_list.begin(),
                                       prohibited_list.end());
  if (prohibited) {
    auto iter = prohibited_set.find(type);
    if (iter != prohibited_set.end()) {
      return;
    }
    prohibited_set.insert(type);
  } else {
    auto iter = prohibited_set.find(type);
    if (iter == prohibited_set.end()) {
      return;
    }
    prohibited_set.erase(iter);
  }
  prohibited_list =
      std::vector<std::string>(prohibited_set.begin(), prohibited_set.end());
  prohibited_technologies = base::JoinString(prohibited_list, ",");
  stub_properties_.Set(shill::kProhibitedTechnologiesProperty,
                       prohibited_technologies);
  CallNotifyObserversPropertyChanged(shill::kProhibitedTechnologiesProperty);
}

void FakeShillManagerClient::SetTechnologyEnabled(const std::string& type,
                                                  base::OnceClosure callback,
                                                  bool enabled) {
  base::Value::List& enabled_list =
      GetListProperty(shill::kEnabledTechnologiesProperty);
  if (enabled) {
    AppendIfNotPresent(enabled_list, base::Value(type));
  } else {
    enabled_list.EraseValue(base::Value(type));
  }
  CallNotifyObserversPropertyChanged(shill::kEnabledTechnologiesProperty);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
  // May affect available services.
  SortManagerServices(/*notify=*/true);
}

void FakeShillManagerClient::AddGeoNetwork(const std::string& technology,
                                           const base::Value::Dict& network) {
  base::Value::List* list_value = stub_geo_networks_.EnsureList(technology);
  list_value->Append(network.Clone());
}

void FakeShillManagerClient::AddProfile(const std::string& profile_path) {
  const char* key = shill::kProfilesProperty;
  if (AppendIfNotPresent(GetListProperty(key), base::Value(profile_path))) {
    CallNotifyObserversPropertyChanged(key);
  }
}

void FakeShillManagerClient::ClearProperties() {
  stub_properties_.clear();
}

void FakeShillManagerClient::SetManagerProperty(const std::string& key,
                                                const base::Value& value) {
  SetProperty(key, value, base::DoNothing(), base::BindOnce(&LogErrorCallback));
}

base::Value::Dict FakeShillManagerClient::GetStubProperties() {
  return stub_properties_.Clone();
}

void FakeShillManagerClient::AddManagerService(const std::string& service_path,
                                               bool notify_observers) {
  VLOG(2) << "AddManagerService: " << service_path;
  AppendIfNotPresent(GetListProperty(shill::kServiceCompleteListProperty),
                     base::Value(service_path));
  SortManagerServices(/*notify=*/false);
  if (notify_observers) {
    CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
  }
}

void FakeShillManagerClient::RemoveManagerService(
    const std::string& service_path) {
  VLOG(2) << "RemoveManagerService: " << service_path;
  base::Value service_path_value(service_path);
  GetListProperty(shill::kServiceCompleteListProperty)
      .EraseValue(service_path_value);
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
}

void FakeShillManagerClient::RestartTethering() {
  auto my_error_callback = [](const std::string& error_name,
                              const std::string& error_message) {
    LOG(ERROR) << "Unexpected error occurred: " << error_name << " "
               << error_message;
    NOTREACHED();
  };
  DisableTethering(
      base::BindOnce(&FakeShillManagerClient::OnDisableTetheringSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(my_error_callback));
}

void FakeShillManagerClient::OnDisableTetheringSuccess(
    const std::string& result) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::EnableTethering,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::WiFiInterfacePriority::OS_REQUEST,
                     base::DoNothing(), base::DoNothing()),
      interactive_delay_);
}

void FakeShillManagerClient::ClearManagerServices() {
  VLOG(1) << "ClearManagerServices";
  GetListProperty(shill::kServiceCompleteListProperty).clear();
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
  SortManagerServices(/*notify=*/true);
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
  base::Value::List& complete_path_list =
      GetListProperty(shill::kServiceCompleteListProperty);
  base::Value::List prev_complete_path_list = complete_path_list.Clone();

  base::Value::List& visible_services =
      GetListProperty(shill::kServicesProperty);

  // Networks for disabled services get appended to the end without sorting.
  std::vector<std::string> disabled_path_list;

  // Build a list of dictionaries for each service in the list.
  base::Value::List complete_dict_list;
  for (const base::Value& value : complete_path_list) {
    std::string service_path = value.GetString();
    const base::Value::Dict* properties =
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

    base::Value::Dict properties_copy = properties->Clone();
    properties_copy.Set(kPathKey, service_path);
    complete_dict_list.Append(std::move(properties_copy));
  }

  // Sort the service list using the same logic as Shill's Service::Compare.
  std::sort(complete_dict_list.begin(), complete_dict_list.end(),
            CompareNetworks);

  // Rebuild |complete_path_list| and |visible_services| with the new sort
  // order.
  complete_path_list.clear();
  visible_services.clear();
  for (const base::Value& dict : complete_dict_list) {
    std::string service_path = GetStringValue(dict.GetDict(), kPathKey);
    complete_path_list.Append(service_path);
    if (dict.GetDict().FindBool(shill::kVisibleProperty).value_or(false)) {
      visible_services.Append(service_path);
    }
  }
  // Append disabled networks to the end of the complete path list.
  for (const std::string& path : disabled_path_list) {
    complete_path_list.Append(path);
  }

  // Notify observers if the order changed.
  if (notify && complete_path_list != prev_complete_path_list) {
    CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
  }

  // Set the first connected service as the Default service. Note:
  // |new_default_service| may be empty indicating no default network.
  std::string new_default_service;
  if (!complete_dict_list.empty()) {
    const base::Value::Dict& default_network = complete_dict_list[0].GetDict();
    if (IsConnectedState(
            GetStringValue(default_network, shill::kStateProperty))) {
      new_default_service = GetStringValue(default_network, kPathKey);
    }
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
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  network_throttling_status_ = status;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

bool FakeShillManagerClient::GetFastTransitionStatus() {
  std::optional<bool> fast_transition_status = stub_properties_.FindBool(
      std::string_view(shill::kWifiGlobalFTEnabledProperty));
  return fast_transition_status && fast_transition_status.value();
}

void FakeShillManagerClient::SetSimulateConfigurationResult(
    FakeShillSimulatedResult configuration_result) {
  simulate_configuration_result_ = configuration_result;
}

void FakeShillManagerClient::SetSimulateConfigurationError(
    std::string_view error_name,
    std::string_view error_message) {
  simulate_configuration_error_.name = error_name;
  simulate_configuration_error_.message = error_message;
}

void FakeShillManagerClient::SetSimulateTetheringEnableResult(
    FakeShillSimulatedResult tethering_enable_result,
    const std::string& result_string) {
  simulate_tethering_enable_result_ = tethering_enable_result;
  if (simulate_tethering_enable_result_ == FakeShillSimulatedResult::kSuccess) {
    simulate_enable_tethering_result_string_ = result_string;
  }
}

void FakeShillManagerClient::SetSimulateCheckTetheringReadinessResult(
    FakeShillSimulatedResult tethering_readiness_result,
    const std::string& readiness_status) {
  simulate_check_tethering_readiness_result_ = tethering_readiness_result;
  if (simulate_check_tethering_readiness_result_ ==
      FakeShillSimulatedResult::kSuccess) {
    simulate_tethering_readiness_status_ = readiness_status;
  }
}

void FakeShillManagerClient::SetSimulateCreateP2PGroupResult(
    FakeShillSimulatedResult operation_result,
    const std::string& result_code) {
  simulate_create_p2p_group_result_ = operation_result;
  if (simulate_create_p2p_group_result_ == FakeShillSimulatedResult::kSuccess) {
    simulate_create_p2p_group_result_code_ = result_code;
  }
}

void FakeShillManagerClient::SetSimulateDestroyP2PGroupResult(
    FakeShillSimulatedResult operation_result,
    const std::string& result_code) {
  simulate_destroy_p2p_group_result_ = operation_result;
  if (simulate_destroy_p2p_group_result_ ==
      FakeShillSimulatedResult::kSuccess) {
    simulate_destroy_p2p_group_result_code_ = result_code;
  }
}

void FakeShillManagerClient::SetSimulateConnectToP2PGroupResult(
    FakeShillSimulatedResult operation_result,
    const std::string& result_code) {
  simulate_connect_p2p_group_result_ = operation_result;
  if (simulate_connect_p2p_group_result_ ==
      FakeShillSimulatedResult::kSuccess) {
    simulate_connect_p2p_group_result_code_ = result_code;
  }
}

void FakeShillManagerClient::SetSimulateDisconnectFromP2PGroupResult(
    FakeShillSimulatedResult operation_result,
    const std::string& result_code) {
  simulate_disconnect_p2p_group_result_ = operation_result;
  if (simulate_disconnect_p2p_group_result_ ==
      FakeShillSimulatedResult::kSuccess) {
    simulate_disconnect_p2p_group_result_code_ = result_code;
  }
}

void FakeShillManagerClient::SetupDefaultEnvironment() {
  // Bail out from setup if there is no message loop. This will be the common
  // case for tests that are not testing Shill.
  if (!base::SingleThreadTaskRunner::HasCurrentDefault()) {
    return;
  }

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
  base::Value::Dict ipconfig_v4_dictionary;
  ipconfig_v4_dictionary.Set(shill::kAddressProperty, "100.0.0.1");
  ipconfig_v4_dictionary.Set(shill::kGatewayProperty, "100.0.0.2");
  ipconfig_v4_dictionary.Set(shill::kPrefixlenProperty, 1);
  ipconfig_v4_dictionary.Set(shill::kMethodProperty, shill::kTypeIPv4);
  ipconfig_v4_dictionary.Set(shill::kWebProxyAutoDiscoveryUrlProperty,
                             "http://wpad.com/wpad.dat");
  ip_configs->AddIPConfig("ipconfig_v4_path",
                          std::move(ipconfig_v4_dictionary));
  base::Value::Dict ipconfig_v6_dictionary;
  ipconfig_v6_dictionary.Set(shill::kAddressProperty, "0:0:0:0:100:0:0:1");
  ipconfig_v6_dictionary.Set(shill::kMethodProperty, shill::kTypeIPv6);
  ip_configs->AddIPConfig("ipconfig_v6_path",
                          std::move(ipconfig_v6_dictionary));

  bool enabled;
  std::string state;

  // Ethernet
  state = GetInitialStateForType(shill::kTypeEthernet, &enabled);
  if (state == shill::kStateOnline || state == shill::kStateIdle) {
    AddTechnology(shill::kTypeEthernet, enabled);
    devices->AddDevice("/device/eth1", shill::kTypeEthernet, "stub_eth_device1",
                       "0123456789ab");
    base::Value::List eth_ip_configs;
    eth_ip_configs.Append("ipconfig_v4_path");
    eth_ip_configs.Append("ipconfig_v6_path");
    SetInitialDeviceProperty("/device/eth1", shill::kIPConfigsProperty,
                             base::Value(std::move(eth_ip_configs)));
    const std::string kFakeEthernetNetworkPath = "/service/eth1";
    services->AddService(kFakeEthernetNetworkPath, kFakeEthernetNetworkGuid,
                         "eth1" /* name */, shill::kTypeEthernet, state,
                         add_to_visible);
    profiles->AddService(shared_profile, kFakeEthernetNetworkPath);
  }

  // Wifi
  state = GetInitialStateForType(shill::kTypeWifi, &enabled);
  if (state != kTechnologyUnavailable) {
    bool portaled = false;
    std::string portal_state;
    if (IsPortalledState(state)) {
      portaled = true;
      portal_state = state;
      state = shill::kStateIdle;
    }
    AddTechnology(shill::kTypeWifi, enabled);
    devices->AddDevice("/device/wifi1", shill::kTypeWifi, "stub_wifi_device1",
                       "23456789abcd");
    base::Value::List wifi_ip_configs;
    wifi_ip_configs.Append("ipconfig_v4_path");
    wifi_ip_configs.Append("ipconfig_v6_path");
    SetInitialDeviceProperty("/device/wifi1", shill::kIPConfigsProperty,
                             base::Value(std::move(wifi_ip_configs)));

    const std::string kWifi1Path = "/service/wifi1";
    services->AddService(kWifi1Path, "wifi1_guid", "wifi1" /* name */,
                         shill::kTypeWifi, state, add_to_visible);
    services->SetServiceProperty(kWifi1Path, shill::kSecurityClassProperty,
                                 base::Value(shill::kSecurityClassWep));
    services->SetServiceProperty(kWifi1Path, shill::kConnectableProperty,
                                 base::Value(true));
    profiles->AddService(shared_profile, kWifi1Path);

    const std::string kWifi2Path = "/service/wifi2";
    services->AddService(kWifi2Path, "wifi2_guid",
                         dynamic_wep_ ? "wifi2_WEP" : "wifi2_PSK" /* name */,
                         shill::kTypeWifi, shill::kStateIdle, add_to_visible);
    if (dynamic_wep_) {
      services->SetServiceProperty(kWifi2Path, shill::kSecurityClassProperty,
                                   base::Value(shill::kSecurityClassWep));
      services->SetServiceProperty(kWifi2Path, shill::kEapKeyMgmtProperty,
                                   base::Value(shill::kKeyManagementIEEE8021X));
      services->SetServiceProperty(kWifi2Path, shill::kEapMethodProperty,
                                   base::Value(shill::kEapMethodPEAP));
      services->SetServiceProperty(kWifi2Path, shill::kEapIdentityProperty,
                                   base::Value("John Doe"));
    } else {
      services->SetServiceProperty(kWifi2Path, shill::kSecurityClassProperty,
                                   base::Value(shill::kSecurityClassPsk));
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
                                   base::Value(shill::kSecurityClassNone));
      services->SetConnectBehavior(
          kPortaledWifiPath,
          base::BindRepeating(&UpdatePortaledState, kPortaledWifiPath,
                              portal_state));
      services->SetServiceProperty(
          kPortaledWifiPath, shill::kConnectableProperty, base::Value(true));
      profiles->AddService(shared_profile, kPortaledWifiPath);
    }

    for (int i = 0; i < extra_wifi_networks_; ++i) {
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
                       "stub_cellular_device1", "23456789abcd");
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
      services->SetServiceProperty(kCellularServicePath, shill::kIccidProperty,
                                   base::Value("cellular1"));
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

      base::Value::Dict payment_portal;
      payment_portal.Set(shill::kPaymentPortalMethod, "POST");
      payment_portal.Set(shill::kPaymentPortalPostData,
                         "iccid=123&imei=456&mdn=789");
      payment_portal.Set(shill::kPaymentPortalURL, cellular_olp_);
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kPaymentPortalProperty,
                                   base::Value(std::move(payment_portal)));

      std::string shill_roaming_state;
      if (roaming_state_ == kRoamingRequired) {
        shill_roaming_state = shill::kRoamingStateRoaming;
      } else if (roaming_state_.empty()) {
        shill_roaming_state = shill::kRoamingStateHome;
      } else {  // |roaming_state_| is expected to be a valid Shill state.
        shill_roaming_state = roaming_state_;
      }
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kRoamingStateProperty,
                                   base::Value(shill_roaming_state));

      base::Value apn_value{base::Value::Type::DICT};
      base::Value::Dict& apn = apn_value.GetDict();
      apn.Set(shill::kApnProperty, "testapn");
      apn.Set(shill::kApnNameProperty, "Test APN");
      apn.Set(shill::kApnLocalizedNameProperty, "Localized Test APN");
      apn.Set(shill::kApnUsernameProperty, "User1");
      apn.Set(shill::kApnPasswordProperty, "password");
      apn.Set(shill::kApnAuthenticationProperty, "chap");
      base::Value::Dict apn2;
      apn2.Set(shill::kApnProperty, "testapn2");
      services->SetServiceProperty(kCellularServicePath,
                                   shill::kCellularApnProperty, apn_value);
      services->SetServiceProperty(
          kCellularServicePath, shill::kCellularLastGoodApnProperty, apn_value);
      base::Value::List apn_list;
      apn_list.Append(std::move(apn));
      apn_list.Append(std::move(apn2));
      SetInitialDeviceProperty("/device/cellular1",
                               shill::kCellularApnListProperty,
                               base::Value(std::move(apn_list)));

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
    base::Value::Dict provider_properties_openvpn;
    provider_properties_openvpn.Set(shill::kTypeProperty,
                                    shill::kProviderOpenVpn);
    provider_properties_openvpn.Set(shill::kHostProperty, "vpn_host");

    services->AddService("/service/vpn1", "vpn1_guid", "vpn1" /* name */,
                         shill::kTypeVPN, state, add_to_visible);
    services->SetServiceProperty(
        "/service/vpn1", shill::kProviderProperty,
        base::Value(std::move(provider_properties_openvpn)));
    profiles->AddService(shared_profile, "/service/vpn1");

    base::Value::Dict provider_properties_l2tp;
    provider_properties_l2tp.Set(shill::kTypeProperty,
                                 shill::kProviderL2tpIpsec);
    provider_properties_l2tp.Set(shill::kHostProperty, "vpn_host2");

    services->AddService("/service/vpn2", "vpn2_guid", "vpn2" /* name */,
                         shill::kTypeVPN, shill::kStateIdle, add_to_visible);
    services->SetServiceProperty(
        "/service/vpn2", shill::kProviderProperty,
        base::Value(std::move(provider_properties_l2tp)));
  }

  // Additional device states
  for (const auto& iter1 : shill_device_property_map_) {
    std::string device_type = iter1.first;
    std::string device_path = devices->GetDevicePathForType(device_type);
    for (const auto& iter2 : iter1.second) {
      SetInitialDeviceProperty(device_path, iter2.first, iter2.second);
    }
  }
  shill_device_property_map_.clear();

  SetDefaultProperties();

  SortManagerServices(/*notify=*/true);
}

// Private methods

void FakeShillManagerClient::SetDefaultProperties() {
  auto tethering_config =
      base::Value::Dict()
          .Set(shill::kTetheringConfAutoDisableProperty, true)
          .Set(shill::kTetheringConfBandProperty, shill::kBandAll)
          .Set(shill::kTetheringConfSecurityProperty, shill::kSecurityWpa2)
          .Set(shill::kTetheringConfSSIDProperty,
               base::HexEncode(kDefaultTetheringSSID))
          .Set(shill::kTetheringConfPassphraseProperty,
               kDefaultTetheringPassphrase)
          .Set(shill::kTetheringConfMARProperty, true);
  SetManagerProperty(shill::kTetheringConfigProperty,
                     base::Value(std::move(tethering_config)));

  auto tethering_capabilities =
      base::Value::Dict()
          .Set(shill::kTetheringCapUpstreamProperty,
               base::Value::List().Append(shill::kTypeCellular))
          .Set(shill::kTetheringCapDownstreamProperty,
               base::Value::List().Append(shill::kTypeWifi))
          .Set(shill::kTetheringCapSecurityProperty,
               base::Value::List().Append(shill::kSecurityWpa2));
  SetManagerProperty(shill::kTetheringCapabilitiesProperty,
                     base::Value(std::move(tethering_capabilities)));

  auto tethering_state =
      base::Value::Dict()
          .Set(shill::kTetheringStatusStateProperty, shill::kTetheringStateIdle)
          .Set(shill::kTetheringStatusIdleReasonProperty,
               shill::kTetheringIdleReasonInitialState);
  SetManagerProperty(shill::kTetheringStatusProperty,
                     base::Value(std::move(tethering_state)));

  auto wifi_direct_capabilities =
      base::Value::Dict()
          .Set(shill::kP2PCapabilitiesP2PSupportedProperty, true)
          .Set(shill::kP2PCapabilitiesGroupReadinessProperty,
               shill::kP2PCapabilitiesGroupReadinessReady)
          .Set(shill::kP2PCapabilitiesClientReadinessProperty,
               shill::kP2PCapabilitiesClientReadinessReady)
          .Set(shill::kP2PCapabilitiesSupportedChannelsProperty,
               base::Value::List().Append(1).Append(2))
          .Set(shill::kP2PCapabilitiesPreferredChannelsProperty,
               base::Value::List().Append(1));
  SetManagerProperty(shill::kP2PCapabilitiesProperty,
                     base::Value(std::move(wifi_direct_capabilities)));
}

void FakeShillManagerClient::PassNullopt(
    chromeos::DBusMethodCallback<base::Value::Dict> callback) const {
  std::move(callback).Run(std::nullopt);
}

void FakeShillManagerClient::PassStubProperties(
    chromeos::DBusMethodCallback<base::Value::Dict> callback) const {
  base::Value::Dict stub_properties = stub_properties_.Clone();
  stub_properties.Set(shill::kServiceCompleteListProperty,
                      GetEnabledServiceList());
  std::move(callback).Run(std::move(stub_properties));
}

void FakeShillManagerClient::PassStubGeoNetworks(
    chromeos::DBusMethodCallback<base::Value::Dict> callback) const {
  std::move(callback).Run(stub_geo_networks_.Clone());
}

void FakeShillManagerClient::CallNotifyObserversPropertyChanged(
    const std::string& property) {
  // Avoid unnecessary delayed task if we have no observers (e.g. during
  // initial setup).
  if (observer_list_.empty()) {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillManagerClient::NotifyObserversPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(), property));
}

void FakeShillManagerClient::NotifyObserversPropertyChanged(
    const std::string& property) {
  VLOG(1) << "NotifyObserversPropertyChanged: " << property;
  if (!stub_properties_.contains(property)) {
    LOG(ERROR) << "Notify for unknown property: " << property;
    return;
  }
  // Notify using a clone instead of a pointer to the property to avoid the
  // situation where an observer invalidates our pointer when notified.
  const base::Value value = stub_properties_.Find(property)->Clone();
  if (property == shill::kServiceCompleteListProperty) {
    base::Value services(GetEnabledServiceList());
    for (auto& observer : observer_list_) {
      observer.OnPropertyChanged(property, services);
    }
    return;
  }
  for (auto& observer : observer_list_) {
    observer.OnPropertyChanged(property, value);
  }
}

base::Value::List& FakeShillManagerClient::GetListProperty(
    const std::string& property) {
  base::Value::List* list_property = stub_properties_.EnsureList(property);
  return *list_property;
}

bool FakeShillManagerClient::TechnologyEnabled(const std::string& type) const {
  if (type == shill::kTypeVPN) {
    return true;  // VPN is always "enabled" since there is no associated device
  }
  if (type == shill::kTypeEthernetEap) {
    return true;
  }
  const base::Value::List* technologies =
      stub_properties_.FindList(shill::kEnabledTechnologiesProperty);
  if (technologies) {
    return base::Contains(*technologies, base::Value(type));
  }
  return false;
}

base::Value::List FakeShillManagerClient::GetEnabledServiceList() const {
  base::Value::List new_service_list;
  const base::Value::List* service_list =
      stub_properties_.FindList(shill::kServiceCompleteListProperty);
  if (service_list) {
    ShillServiceClient::TestInterface* service_client =
        ShillServiceClient::Get()->GetTestInterface();
    for (const base::Value& v : *service_list) {
      std::string service_path = v.GetString();
      const base::Value::Dict* properties =
          service_client->GetServiceProperties(service_path);
      if (!properties) {
        LOG(ERROR) << "Properties not found for service: " << service_path;
        continue;
      }
      const std::string* type = properties->FindString(shill::kTypeProperty);
      if (type && TechnologyEnabled(*type)) {
        new_service_list.Append(v.Clone());
      }
    }
  }
  return new_service_list;
}

void FakeShillManagerClient::ClearProfiles() {
  if (GetListProperty(shill::kProfilesProperty).empty()) {
    return;
  }
  GetListProperty(shill::kProfilesProperty).clear();
  CallNotifyObserversPropertyChanged(shill::kProfilesProperty);
}

void FakeShillManagerClient::SetShouldReturnNullProperties(bool value) {
  return_null_properties_ = value;
}

void FakeShillManagerClient::SetWifiServicesVisibleByDefault(
    bool wifi_services_visible_by_default) {
  wifi_services_visible_by_default_ = wifi_services_visible_by_default;
}

void FakeShillManagerClient::ScanCompleted(const std::string& device_path) {
  VLOG(1) << "ScanCompleted: " << device_path;
  if (!device_path.empty()) {
    ShillDeviceClient::Get()->GetTestInterface()->SetDeviceProperty(
        device_path, shill::kScanningProperty, base::Value(false),
        /*notify_changed=*/true);
  }
  CallNotifyObserversPropertyChanged(shill::kServiceCompleteListProperty);
  if (connect_to_best_services_callbacks_) {
    // Use PostTask so the ScanAndConnectToBestServices callback is executed
    // after the Scanning property change has been dispatched.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeShillManagerClient::ContinueConnectToBestServices,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(connect_to_best_services_callbacks_.value())));
    connect_to_best_services_callbacks_.reset();
  }
}

void FakeShillManagerClient::ParseCommandLineSwitch() {
  // Default setup
  SetInitialNetworkState(shill::kTypeEthernet, shill::kStateOnline);
  SetInitialNetworkState(shill::kTypeWifi, shill::kStateOnline);
  SetInitialNetworkState(shill::kTypeCellular, shill::kStateIdle);
  SetInitialNetworkState(shill::kTypeVPN, shill::kStateIdle);

  // Parse additional options
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kShillStub)) {
    return;
  }

  std::string option_str =
      command_line->GetSwitchValueASCII(chromeos::switches::kShillStub);
  VLOG(1) << "Parsing command line:" << option_str;
  base::StringPairs string_pairs;
  base::SplitStringIntoKeyValuePairs(option_str, '=', ',', &string_pairs);
  for (auto& string_pair : string_pairs) {
    ParseOption(string_pair.first, string_pair.second);
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
    if (!arg1.empty()) {
      base::StringToInt(arg1, &seconds);
    }
    interactive_delay_ = base::Seconds(seconds);
    return true;
  } else if (arg0 == "sim_lock") {
    bool locked = (arg1 == "1");
    base::Value::Dict simlock_dict;
    simlock_dict.Set(shill::kSIMLockEnabledProperty, true);
    std::string lock_type = locked ? shill::kSIMLockPin : "";
    simlock_dict.Set(shill::kSIMLockTypeProperty, lock_type);
    if (locked) {
      simlock_dict.Set(shill::kSIMLockRetriesLeftProperty,
                       FakeShillDeviceClient::kSimPinRetryCount);
    }
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kSIMPresentProperty] = base::Value(true);
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kSIMLockStatusProperty] =
                                  base::Value(std::move(simlock_dict));
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kTechnologyFamilyProperty] =
                                  base::Value(shill::kNetworkTechnologyGsm);
    return true;
  } else if (arg0 == "sim_present") {
    bool present = (arg1 == "1");
    shill_device_property_map_[shill::kTypeCellular]
                              [shill::kSIMPresentProperty] =
                                  base::Value(present);
    if (!present) {
      shill_initial_state_map_[shill::kTypeCellular] = kNetworkDisabled;
    }
    return true;
  } else if (arg0 == "olp") {
    cellular_olp_ = arg1;
    return true;
  } else if (arg0 == "roaming") {
    // "home", "roaming", or "required"
    roaming_state_ = arg1;
    return true;
  } else if (arg0 == "dynamic_wep" && arg1 == "1") {
    dynamic_wep_ = true;
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
    extra_wifi_networks_ = state_arg_as_int - 1;
  } else if (state_arg == "disabled" || state_arg == "disconnect") {
    // Technology disabled but available, services created but not connected.
    state = kNetworkDisabled;
  } else if (state_arg == "none" || state_arg == "offline") {
    // Technology not available, do not create services.
    state = kTechnologyUnavailable;
  } else if (state_arg == "initializing") {
    // Technology available but not initialized.
    state = kTechnologyInitializing;
  } else if (state_arg == "redirect-found" || state_arg == "portal") {
    // Technology is enabled, a service is connected and in redirect-found
    // state.
    state = shill::kStateRedirectFound;
  } else if (state_arg == "portal-suspected") {
    // Technology is enabled, a service is connected and in portal-suspected
    // state.
    state = shill::kStatePortalSuspected;
  } else if (state_arg == "no-connectivity") {
    // Technology is enabled, a service is connected and in no-connectivity
    // state.
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
  if (type_arg == "eth") {
    type_arg = shill::kTypeEthernet;
  }

  if (type_arg != shill::kTypeEthernet && type_arg != shill::kTypeWifi &&
      type_arg != shill::kTypeCellular && type_arg != shill::kTypeVPN) {
    LOG(WARNING) << "Unrecognized Shill network type: " << type_arg;
    return false;
  }

  // Disabled ethernet is the same as unavailable.
  if (type_arg == shill::kTypeEthernet && state == kNetworkDisabled) {
    state = kTechnologyUnavailable;
  }

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

}  // namespace ash
