// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

void CallSortManagerServices() {
  ShillManagerClient::Get()->GetTestInterface()->SortManagerServices(true);
}

base::TimeDelta GetInteractiveDelay() {
  return ShillManagerClient::Get()->GetTestInterface()->GetInteractiveDelay();
}

// Extracts the hex SSID from shill |service_properties|.
std::string GetHexSSID(const base::Value::Dict& service_properties) {
  const std::string* hex_ssid =
      service_properties.FindString(shill::kWifiHexSsid);
  if (hex_ssid) {
    return *hex_ssid;
  }
  const std::string* ssid = service_properties.FindString(shill::kSSIDProperty);
  if (ssid) {
    return base::HexEncode(*ssid);
  }
  return std::string();
}

std::string GetSecurityClass(const base::Value::Dict& service_properties) {
  // Mimics shill's WiFiProvider::GetServiceParametersFromStorage with
  // WiFiService::ComputeSecurityClass.
  const std::string* security_class =
      service_properties.FindString(shill::kSecurityClassProperty);

  if (security_class) {
    return *security_class;
  }

  const std::string* security =
      service_properties.FindString(shill::kSecurityProperty);

  if (!security) {
    return shill::kSecurityClassNone;
  }

  static const std::array<std::string, 6> psk_securities = {
      shill::kSecurityWpa,  shill::kSecurityWpaWpa2,  shill::kSecurityWpaAll,
      shill::kSecurityWpa2, shill::kSecurityWpa2Wpa3, shill::kSecurityWpa3};
  if (base::Contains(psk_securities, *security)) {
    return shill::kSecurityClassPsk;
  }

  static const std::array<std::string, 6> eap_securities = {
      shill::kSecurityWpaEnterprise,      shill::kSecurityWpaWpa2Enterprise,
      shill::kSecurityWpaAllEnterprise,   shill::kSecurityWpa2Enterprise,
      shill::kSecurityWpa2Wpa3Enterprise, shill::kSecurityWpa3Enterprise};
  if (base::Contains(eap_securities, *security)) {
    return shill::kSecurityClass8021x;
  }

  // Neither PSK nor 8021x so it is either "wep" or "none" securities which
  // are the same as their SecurityClass names.
  return *security;
}

// Returns true if both |template_service_properties| and |service_properties|
// have the key |key| and both have the same value for it.
bool HaveSameValueForKey(const base::Value::Dict& template_service_properties,
                         const base::Value::Dict& service_properties,
                         std::string_view key) {
  const base::Value* template_service_value =
      template_service_properties.Find(key);
  const base::Value* service_value = service_properties.Find(key);
  return template_service_value && service_value &&
         *template_service_value == *service_value;
}

// Mimics shill's similar service matching logic. This is only invoked if
// |template_service_properties| and |service_properties| refer to a service of
// the same type.
bool IsSimilarService(const std::string& service_type,
                      const base::Value::Dict& template_service_properties,
                      const base::Value::Dict& service_properties) {
  if (service_type == shill::kTypeWifi) {
    // Mimics shill's WiFiProvider::FindSimilarService.
    return GetHexSSID(template_service_properties) ==
               GetHexSSID(service_properties) &&
           HaveSameValueForKey(template_service_properties, service_properties,
                               shill::kModeProperty) &&
           GetSecurityClass(template_service_properties) ==
               GetSecurityClass(service_properties);
  }

  // Assume that Ethernet / EthernetEAP services are always similar.
  if (service_type == shill::kTypeEthernet ||
      service_type == shill::kTypeEthernetEap) {
    // Mimics shill's EthernetProvider::FindSimilarService.
    return true;
  }

  return false;
}

// Properties that should be retained when a visible service is deleted from a
// profile, i.e. when all its configured properties are removed. This should
// contain properties which can be "observed", e.g. a SSID.
// For simplicity, these are not distinguished by service type.
constexpr const char* kIntrinsicServiceProperties[] = {
    shill::kTypeProperty,
    shill::kDeviceProperty,
    shill::kVisibleProperty,
    shill::kStateProperty,
    shill::kSSIDProperty,
    shill::kWifiHexSsid,
    shill::kSignalStrengthProperty,
    shill::kWifiFrequency,
    shill::kWifiFrequencyListProperty,
    shill::kWifiHexSsid,
    shill::kModeProperty,
    shill::kSecurityProperty,
    shill::kSecurityClassProperty,
    shill::kNetworkTechnologyProperty,
    shill::kNameProperty,
    shill::kProviderProperty};

}  // namespace

FakeShillServiceClient::FakeShillServiceClient() {
  SetDefaultFakeTrafficCounters();
}

FakeShillServiceClient::~FakeShillServiceClient() = default;

// ShillServiceClient overrides.

void FakeShillServiceClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& service_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(service_path).AddObserver(observer);
}

void FakeShillServiceClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& service_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(service_path).RemoveObserver(observer);
}

void FakeShillServiceClient::GetProperties(
    const dbus::ObjectPath& service_path,
    chromeos::DBusMethodCallback<base::Value::Dict> callback) {
  std::optional<base::Value::Dict> result_properties;
  const base::Value::Dict* nested_dict =
      GetServiceProperties(service_path.value());
  if (nested_dict) {
    result_properties = nested_dict->Clone();
    // Remove credentials that Shill wouldn't send.
    result_properties->Remove(shill::kPassphraseProperty);
  } else {
    DCHECK(!require_service_to_get_properties_);

    // This may happen if we remove services from the list.
    VLOG(2) << "Properties not found for: " << service_path.value();
  }

  base::OnceClosure property_update =
      base::BindOnce(std::move(callback), std::move(result_properties));
  if (hold_back_service_property_updates_) {
    recorded_property_updates_.push_back(std::move(property_update));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(property_update));
  }
}

void FakeShillServiceClient::SetProperty(const dbus::ObjectPath& service_path,
                                         const std::string& name,
                                         const base::Value& value,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) {
  if (!SetServiceProperty(service_path.value(), name, value)) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillServiceClient::SetProperties(const dbus::ObjectPath& service_path,
                                           const base::Value::Dict& properties,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  if (set_properties_error_name_.has_value()) {
    std::move(error_callback)
        .Run(set_properties_error_name_.value(),
             /*error_message=*/std::string());
    set_properties_error_name_ = std::nullopt;
    return;
  }
  for (auto iter : properties) {
    if (!SetServiceProperty(service_path.value(), iter.first, iter.second)) {
      LOG(ERROR) << "Service not found: " << service_path.value();
      std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
      return;
    }
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillServiceClient::ClearProperty(const dbus::ObjectPath& service_path,
                                           const std::string& name,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  base::Value::Dict* dict = GetModifiableServiceProperties(
      service_path.value(), /*create_if_missing=*/false);
  if (!dict) {
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }
  dict->Remove(name);
  // Note: Shill does not send notifications when properties are cleared.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillServiceClient::ClearProperties(
    const dbus::ObjectPath& service_path,
    const std::vector<std::string>& names,
    ListValueCallback callback,
    ErrorCallback error_callback) {
  base::Value::Dict* dict = GetModifiableServiceProperties(
      service_path.value(), /*create_if_missing=*/false);
  if (!dict) {
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }

  base::Value::List result;
  for (const auto& name : names) {
    // Note: Shill does not send notifications when properties are cleared.
    result.Append(dict->Remove(name));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void FakeShillServiceClient::Connect(const dbus::ObjectPath& service_path,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) {
  VLOG(1) << "FakeShillServiceClient::Connect: " << service_path.value();
  base::Value::Dict* service_properties = GetModifiableServiceProperties(
      service_path.value(), /*create_if_missing=*/false);
  if (!service_properties) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }

  // This should be a no-op if it's already connecting or connected.
  const std::string* state =
      service_properties->FindString(shill::kStateProperty);
  if (state &&
      (*state == shill::kStateAssociation || *state == shill::kStateOnline)) {
    std::move(callback).Run();
    return;
  }

  if (connect_error_name_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback), *connect_error_name_,
                       /*error_message=*/std::string()));
    connect_error_name_ = std::nullopt;
    return;
  }

  // Set any other services of the same Type to 'offline' first, before setting
  // State to Association which will trigger sorting Manager.Services and
  // sending an update.
  SetOtherServicesOffline(service_path.value());

  // Clear Error.
  service_properties->Set(shill::kErrorProperty, "");

  // Set Associating.
  base::Value associating_value(shill::kStateAssociation);
  SetServiceProperty(service_path.value(), shill::kStateProperty,
                     associating_value);

  // Stay Associating until the state is changed again after a delay.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillServiceClient::ContinueConnect,
                     weak_ptr_factory_.GetWeakPtr(), service_path.value()),
      GetInteractiveDelay());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillServiceClient::Disconnect(const dbus::ObjectPath& service_path,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  const base::Value::Dict* service = GetServiceProperties(service_path.value());
  if (!service) {
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }
  // Set Idle after a delay
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillServiceClient::SetProperty,
                     weak_ptr_factory_.GetWeakPtr(), service_path,
                     shill::kStateProperty, base::Value(shill::kStateIdle),
                     std::move(callback), std::move(error_callback)),
      GetInteractiveDelay());
}

void FakeShillServiceClient::Remove(const dbus::ObjectPath& service_path,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillServiceClient::CompleteCellularActivation(
    const dbus::ObjectPath& service_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillServiceClient::GetLoadableProfileEntries(
    const dbus::ObjectPath& service_path,
    chromeos::DBusMethodCallback<base::Value::Dict> callback) {
  ShillProfileClient::TestInterface* profile_client =
      ShillProfileClient::Get()->GetTestInterface();
  std::vector<std::string> profiles;
  profile_client->GetProfilePathsContainingService(service_path.value(),
                                                   &profiles);

  DCHECK(profiles.size()) << "No profiles contain given service";
  // Provide a dictionary with {profile_path: service_path} entries for
  // profile_paths that contain the service.
  base::Value::Dict result_properties;
  for (const auto& profile : profiles) {
    result_properties.Set(profile, service_path.value());
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(result_properties)));
}

void FakeShillServiceClient::GetWiFiPassphrase(
    const dbus::ObjectPath& service_path,
    StringCallback callback,
    ErrorCallback error_callback) {
  base::Value::Dict* service_properties =
      GetModifiableServiceProperties(service_path.value(), false);
  if (!service_properties) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }

  const std::string* passphrase =
      service_properties->FindString(shill::kPassphraseProperty);
  if (!passphrase) {
    std::move(error_callback)
        .Run("Error.PassphraseNotFound", "Passphrase not found");
    return;
  }
  std::move(callback).Run(*passphrase);
}

void FakeShillServiceClient::GetEapPassphrase(
    const dbus::ObjectPath& service_path,
    StringCallback callback,
    ErrorCallback error_callback) {
  base::Value::Dict* service_properties =
      GetModifiableServiceProperties(service_path.value(), false);
  if (!service_properties) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    std::move(error_callback).Run("Error.InvalidService", "Invalid Service");
    return;
  }

  const std::string* passphrase =
      service_properties->FindString(shill::kEapPasswordProperty);
  std::move(callback).Run(passphrase ? *passphrase : std::string());
}

void FakeShillServiceClient::RequestPortalDetection(
    const dbus::ObjectPath& service_path,
    chromeos::VoidDBusMethodCallback callback) {
  if (request_portal_state_) {
    SetServiceProperty(service_path.value(), shill::kStateProperty,
                       base::Value(*request_portal_state_));
    request_portal_state_ = std::nullopt;
  }
  std::move(callback).Run(/*success=*/true);
}

void FakeShillServiceClient::RequestTrafficCounters(
    const dbus::ObjectPath& service_path,
    chromeos::DBusMethodCallback<base::Value> callback) {
  std::move(callback).Run(base::Value(fake_traffic_counters_.Clone()));
}

void FakeShillServiceClient::ResetTrafficCounters(
    const dbus::ObjectPath& service_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  fake_traffic_counters_.clear();
  base::Time reset_time =
      !time_getter_.is_null() ? time_getter_.Run() : base::Time::Now();
  SetServiceProperty(
      service_path.value(), shill::kTrafficCounterResetTimeProperty,
      base::Value(reset_time.ToDeltaSinceWindowsEpoch().InMillisecondsF()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

ShillServiceClient::TestInterface* FakeShillServiceClient::GetTestInterface() {
  return this;
}

// ShillServiceClient::TestInterface overrides.
void FakeShillServiceClient::AddService(const std::string& service_path,
                                        const std::string& guid,
                                        const std::string& name,
                                        const std::string& type,
                                        const std::string& state,
                                        bool visible) {
  AddServiceWithIPConfig(service_path, guid, name, type, state,
                         std::string() /* ipconfig_path */, visible);
}

void FakeShillServiceClient::AddServiceWithIPConfig(
    const std::string& service_path,
    const std::string& guid,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    const std::string& ipconfig_path,
    bool visible) {
  base::Value::Dict* properties =
      SetServiceProperties(service_path, guid, name, type, state, visible);

  if (!ipconfig_path.empty()) {
    properties->Set(shill::kIPConfigProperty, ipconfig_path);
  }

  ShillManagerClient::Get()->GetTestInterface()->AddManagerService(service_path,
                                                                   true);
}

base::Value::Dict* FakeShillServiceClient::SetServiceProperties(
    const std::string& service_path,
    const std::string& guid,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    bool visible) {
  base::Value::Dict* properties =
      GetModifiableServiceProperties(service_path, true);
  connect_behavior_.erase(service_path);

  // If |guid| is provided, set Service.GUID to that. Otherwise if a GUID is
  // stored in a profile entry, use that. Otherwise leave it blank. Shill does
  // not enforce a valid guid, we do that at the NetworkStateHandler layer.
  std::string guid_to_set = guid;
  if (guid_to_set.empty()) {
    std::string profile_path;
    std::optional<base::Value::Dict> profile_properties =
        ShillProfileClient::Get()->GetTestInterface()->GetService(
            service_path, &profile_path);
    if (profile_properties) {
      const std::string* profile_guid =
          profile_properties->FindString(shill::kGuidProperty);
      if (profile_guid) {
        guid_to_set = *profile_guid;
      }
    }
  }
  if (!guid_to_set.empty()) {
    properties->Set(shill::kGuidProperty, guid_to_set);
  }
  if (type == shill::kTypeWifi) {
    properties->Set(shill::kSSIDProperty, name);
    properties->Set(shill::kWifiHexSsid, base::HexEncode(name));
  }
  properties->Set(shill::kNameProperty, name);
  std::string device_path =
      ShillDeviceClient::Get()->GetTestInterface()->GetDevicePathForType(type);
  properties->Set(shill::kDeviceProperty, device_path);
  properties->Set(shill::kTypeProperty, type);
  properties->Set(shill::kStateProperty, state);
  properties->Set(shill::kVisibleProperty, visible);
  if (type == shill::kTypeWifi) {
    properties->Set(shill::kSecurityClassProperty, shill::kSecurityClassNone);
    properties->Set(shill::kModeProperty, shill::kModeManaged);
  }

  // Ethernet is always connectable.
  if (type == shill::kTypeEthernet) {
    properties->Set(shill::kConnectableProperty, true);
  }

  // Cellular is always metered.
  if (type == shill::kTypeCellular) {
    properties->Set(shill::kMeteredProperty, true);
    properties->Set(shill::kTrafficCounterResetTimeProperty, 0.0);
  }

  return properties;
}

void FakeShillServiceClient::RemoveService(const std::string& service_path) {
  stub_services_.Remove(service_path);
  connect_behavior_.erase(service_path);
  ShillManagerClient::Get()->GetTestInterface()->RemoveManagerService(
      service_path);
}

bool FakeShillServiceClient::SetServiceProperty(const std::string& service_path,
                                                const std::string& property,
                                                const base::Value& value) {
  base::Value::Dict* dict =
      GetModifiableServiceProperties(service_path, /*create_if_missing=*/false);
  if (!dict) {
    return false;
  }

  VLOG(1) << "Service.SetProperty: " << property << " = " << value
          << " For: " << service_path;

  std::string changed_property;
  base::CompareCase case_sensitive = base::CompareCase::SENSITIVE;
  if (base::StartsWith(property, "Provider.", case_sensitive) ||
      base::StartsWith(property, "OpenVPN.", case_sensitive) ||
      base::StartsWith(property, "L2TPIPsec.", case_sensitive)) {
    // These properties are only nested within the Provider dictionary if read
    // from Shill. Properties that start with "Provider" need to have that
    // stripped off, other properties are nested in the "Provider" dictionary
    // as-is.
    std::string key = property;
    if (base::StartsWith(property, "Provider.", case_sensitive)) {
      key = property.substr(strlen("Provider."));
    }
    base::Value::Dict* provider = dict->EnsureDict(shill::kProviderProperty);
    provider->Set(key, value.Clone());
    changed_property = shill::kProviderProperty;
  } else {
    dict->Set(property, value.Clone());
    changed_property = property;
  }

  if (changed_property == shill::kPassphraseProperty ||
      changed_property == shill::kSecurityClassProperty) {
    const std::string* passphrase =
        dict->FindString(shill::kPassphraseProperty);
    const std::string* security =
        dict->FindString(shill::kSecurityClassProperty);
    // Make PSK networks connectable if 'Passphrase' is set.
    if (passphrase && !passphrase->empty()) {
      dict->Set(shill::kPassphraseRequiredProperty, false);
      if (security && *security == shill::kSecurityClassPsk) {
        dict->Set(shill::kConnectableProperty, true);
      }
    }
    // Make open networks always connectable.
    if (security && *security == shill::kSecurityClassNone) {
      dict->Set(shill::kConnectableProperty, true);
    }
  }

  // Add or update the profile entry.
  ShillProfileClient::TestInterface* profile_test =
      ShillProfileClient::Get()->GetTestInterface();
  if (property == shill::kProfileProperty) {
    const std::string* profile_path = value.GetIfString();
    if (profile_path) {
      if (!profile_path->empty()) {
        profile_test->AddService(*profile_path, service_path);
      }
    } else {
      LOG(ERROR) << "Profile value is not a String!";
    }
  } else {
    const std::string* profile_path = dict->FindString(shill::kProfileProperty);
    if (profile_path && !profile_path->empty()) {
      profile_test->UpdateService(*profile_path, service_path);
    }
  }

  if (property == shill::kCellularCustomApnListProperty) {
    const std::string* type = dict->FindString(shill::kTypeProperty);
    CHECK(type && *type == shill::kTypeCellular);
    bool deafult_apn_found = false;
    // Set connected APN to the latest custom default type APN to simulate the
    // behavior in Shill. When no default type custom APNs are found, the
    // default Modb APN should be used for connection.
    for (const auto& custom_apn : value.GetList()) {
      CHECK(custom_apn.is_dict());
      const std::string* apn_types =
          custom_apn.GetDict().FindString(shill::kApnTypesProperty);
      if (!apn_types || *apn_types == shill::kApnTypeIA) {
        continue;
      }
      dict->Set(shill::kCellularLastGoodApnProperty, custom_apn.Clone());
      deafult_apn_found = true;
      break;
    }
    if (!deafult_apn_found) {
      dict->Set(shill::kCellularLastGoodApnProperty,
                GetFakeDefaultModbApnDict());
    }
  }

  // Notify the Manager if the state changed (affects DefaultService).
  if (property == shill::kStateProperty) {
    ShillManagerClient::Get()->GetTestInterface()->ServiceStateChanged(
        service_path, value.is_string() ? value.GetString() : std::string());
  }

  // If the State or Visibility changes, the sort order of service lists may
  // change and the DefaultService property may change.
  if (property == shill::kStateProperty ||
      property == shill::kVisibleProperty) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CallSortManagerServices));
  }

  // Notify Chrome of the property change.
  base::OnceClosure property_update =
      base::BindOnce(&FakeShillServiceClient::NotifyObserversPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     dbus::ObjectPath(service_path), changed_property);
  if (hold_back_service_property_updates_) {
    recorded_property_updates_.push_back(std::move(property_update));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(property_update));
  }
  return true;
}

const base::Value::Dict* FakeShillServiceClient::GetServiceProperties(
    const std::string& service_path) const {
  return stub_services_.FindDict(service_path);
}

bool FakeShillServiceClient::ClearConfiguredServiceProperties(
    const std::string& service_path) {
  base::Value::Dict* service_dict = GetModifiableServiceProperties(
      service_path, false /* create_if_missing */);
  if (!service_dict) {
    return false;
  }

  const std::optional<bool> visible_property =
      service_dict->FindBool(shill::kVisibleProperty);
  const std::string* service_type =
      service_dict->FindString(shill::kTypeProperty);
  if (!visible_property || !visible_property.value() || !service_type ||
      (*service_type == shill::kTypeVPN) ||
      (*service_type == shill::kTypeCellular)) {
    stub_services_.Remove(service_path);
    RemoveService(service_path);
    return true;
  }

  base::Value::Dict properties_after_delete_entry;

  // Explicitly clear the profile property using SetServiceProperty so a
  // notification is sent about that.
  SetServiceProperty(service_path, shill::kProfileProperty,
                     base::Value(std::string()));
  properties_after_delete_entry.Set(shill::kProfileProperty, std::string());

  for (const std::string& property_to_retain : kIntrinsicServiceProperties) {
    const base::Value* value = service_dict->Find(property_to_retain);
    if (!value) {
      continue;
    }
    properties_after_delete_entry.Set(property_to_retain, value->Clone());
  }
  stub_services_.Set(service_path, std::move(properties_after_delete_entry));
  return true;
}

base::Value::Dict FakeShillServiceClient::GetFakeDefaultModbApnDict() {
  return base::Value::Dict()
      .Set(shill::kApnProperty, "default_apn")
      .Set(shill::kApnNameProperty, "default_apn")
      .Set(shill::kApnLocalizedNameProperty, "localized test apn")
      .Set(shill::kApnUsernameProperty, "user name")
      .Set(shill::kApnPasswordProperty, "password")
      .Set(shill::kApnAuthenticationProperty, "chap")
      .Set(shill::kApnTypesProperty, shill::kApnTypeDefault)
      .Set(shill::kApnSourceProperty, shill::kApnSourceMoDb);
}

std::string FakeShillServiceClient::FindServiceMatchingGUID(
    const std::string& guid) {
  for (const auto [service_path, service_properties] : stub_services_) {
    const std::string* service_guid =
        service_properties.GetDict().FindString(shill::kGuidProperty);
    if (service_guid && *service_guid == guid) {
      return service_path;
    }
  }

  return std::string();
}

std::string FakeShillServiceClient::FindServiceMatchingName(
    const std::string& name) {
  for (const auto [service_path, service_properties] : stub_services_) {
    const std::string* service_name =
        service_properties.GetDict().FindString(shill::kNameProperty);
    if (service_name && *service_name == name) {
      return service_path;
    }
  }

  return std::string();
}

std::string FakeShillServiceClient::FindSimilarService(
    const base::Value::Dict& template_service_properties) {
  const std::string* template_type =
      template_service_properties.FindString(shill::kTypeProperty);
  if (!template_type) {
    return std::string();
  }

  for (const auto service_pair : stub_services_) {
    const auto& service_path = service_pair.first;
    const base::Value::Dict& service_properties = service_pair.second.GetDict();

    const std::string* service_type =
        service_properties.FindString(shill::kTypeProperty);
    if (!service_type || *service_type != *template_type) {
      continue;
    }

    if (IsSimilarService(*service_type, template_service_properties,
                         service_properties)) {
      return service_path;
    }
  }

  return std::string();
}

void FakeShillServiceClient::ClearServices() {
  ShillManagerClient::Get()->GetTestInterface()->ClearManagerServices();
  stub_services_.clear();
  connect_behavior_.clear();
}

void FakeShillServiceClient::SetConnectBehavior(
    const std::string& service_path,
    const base::RepeatingClosure& behavior) {
  if (!behavior) {
    connect_behavior_.erase(service_path);
    return;
  }
  connect_behavior_[service_path] = behavior;
}

void FakeShillServiceClient::SetErrorForNextConnectionAttempt(
    const std::string& error_name) {
  connect_error_name_ = error_name;
}

void FakeShillServiceClient::SetErrorForNextSetPropertiesAttempt(
    const std::string& error_name) {
  set_properties_error_name_ = error_name;
}

void FakeShillServiceClient::SetRequestPortalState(const std::string& state) {
  request_portal_state_ = state;
}

void FakeShillServiceClient::SetHoldBackServicePropertyUpdates(bool hold_back) {
  hold_back_service_property_updates_ = hold_back;
  std::vector<base::OnceClosure> property_updates;
  recorded_property_updates_.swap(property_updates);

  if (hold_back_service_property_updates_) {
    return;
  }

  for (auto& property_update : property_updates) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(property_update));
  }
}

void FakeShillServiceClient::SetRequireServiceToGetProperties(
    bool require_service_to_get_properties) {
  require_service_to_get_properties_ = require_service_to_get_properties;
}

void FakeShillServiceClient::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& service_path,
    const std::string& property) {
  std::string path = service_path.value();
  const base::Value::Dict* dict = GetServiceProperties(path);
  if (!dict) {
    LOG(ERROR) << "Notify for unknown service: " << path;
    return;
  }
  const base::Value* value = dict->Find(property);
  if (!value) {
    LOG(ERROR) << "Notify for unknown property: " << path << " : " << property;
    return;
  }
  for (auto& observer : GetObserverList(service_path)) {
    observer.OnPropertyChanged(property, *value);
  }
}

base::Value::Dict* FakeShillServiceClient::GetModifiableServiceProperties(
    const std::string& service_path,
    bool create_if_missing) {
  base::Value::Dict* properties = stub_services_.FindDict(service_path);
  if (!properties && create_if_missing) {
    properties = stub_services_.EnsureDict(service_path);
  }
  return properties;
}

FakeShillServiceClient::PropertyObserverList&
FakeShillServiceClient::GetObserverList(const dbus::ObjectPath& device_path) {
  auto iter = observer_list_.find(device_path);
  if (iter != observer_list_.end()) {
    return *(iter->second);
  }
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = base::WrapUnique(observer_list);
  return *observer_list;
}

void FakeShillServiceClient::SetOtherServicesOffline(
    const std::string& service_path) {
  const base::Value::Dict* service_properties =
      GetServiceProperties(service_path);
  if (!service_properties) {
    LOG(ERROR) << "Missing service: " << service_path;
    return;
  }
  const std::string* service_type =
      service_properties->FindString(shill::kTypeProperty);
  if (!service_type) {
    return;
  }

  // Set all other services of the same type to offline (Idle).
  for (auto iter : stub_services_) {
    const std::string& path = iter.first;
    if (path == service_path) {
      continue;
    }
    base::Value& properties = iter.second;
    const std::string* type =
        properties.GetDict().FindString(shill::kTypeProperty);
    if (!type || *type != *service_type) {
      continue;
    }

    properties.GetDict().Set(shill::kStateProperty, shill::kStateIdle);
  }
}

void FakeShillServiceClient::SetCellularActivated(
    const dbus::ObjectPath& service_path,
    ErrorCallback error_callback) {
  SetProperty(service_path, shill::kActivationStateProperty,
              base::Value(shill::kActivationStateActivated), base::DoNothing(),
              std::move(error_callback));
  SetProperty(service_path, shill::kConnectableProperty, base::Value(true),
              base::DoNothing(), std::move(error_callback));
}

void FakeShillServiceClient::ContinueConnect(const std::string& service_path) {
  VLOG(1) << "FakeShillServiceClient::ContinueConnect: " << service_path;
  const base::Value::Dict* service_properties =
      GetServiceProperties(service_path);
  if (!service_properties) {
    LOG(ERROR) << "Service not found: " << service_path;
    return;
  }

  if (base::Contains(connect_behavior_, service_path)) {
    const base::RepeatingClosure& custom_connect_behavior =
        connect_behavior_[service_path];
    VLOG(1) << "Running custom connect behavior for " << service_path;
    custom_connect_behavior.Run();
    return;
  }

  // No custom connect behavior set, continue with the default connect behavior.
  const std::string* passphrase =
      service_properties->FindString(shill::kPassphraseProperty);
  if (passphrase && *passphrase == "failure") {
    // Simulate a password failure.
    SetServiceProperty(service_path, shill::kErrorProperty,
                       base::Value(shill::kErrorBadPassphrase));
    SetServiceProperty(service_path, shill::kStateProperty,
                       base::Value(shill::kStateFailure));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&FakeShillServiceClient::SetServiceProperty),
            weak_ptr_factory_.GetWeakPtr(), service_path, shill::kErrorProperty,
            base::Value(shill::kErrorBadPassphrase)));
    return;
  }

  // Set other services of the same type that were previously not "idle" to
  // "idle".
  const std::string* service_type =
      service_properties->FindString(shill::kTypeProperty);
  for (const auto service_pair : stub_services_) {
    const auto& other_service_path = service_pair.first;
    if (other_service_path == service_path) {
      continue;
    }

    const base::Value::Dict& other_service_properties =
        service_pair.second.GetDict();
    const std::string* other_service_type =
        other_service_properties.FindString(shill::kTypeProperty);
    if (!service_type || !other_service_type ||
        *other_service_type != *service_type) {
      continue;
    }
    const std::string* other_service_state =
        other_service_properties.FindString(shill::kStateProperty);
    if (!other_service_state || *other_service_type == shill::kStateIdle) {
      continue;
    }
    SetServiceProperty(other_service_path, shill::kStateProperty,
                       base::Value(shill::kStateIdle));
  }

  // Set the connected service to "online".
  VLOG(1) << "Setting state to Online " << service_path;
  SetServiceProperty(service_path, shill::kStateProperty,
                     base::Value(shill::kStateOnline));
}

void FakeShillServiceClient::SetDefaultFakeTrafficCounters() {
  base::Value::List traffic_counters;

  base::Value::Dict chrome_dict;
  chrome_dict.Set("source", shill::kTrafficCounterSourceChrome);
  chrome_dict.Set("rx_bytes", 1000);
  chrome_dict.Set("tx_bytes", 2000.5);
  traffic_counters.Append(std::move(chrome_dict));

  base::Value::Dict user_dict;
  user_dict.Set("source", shill::kTrafficCounterSourceUser);
  user_dict.Set("rx_bytes", 45);
  user_dict.Set("tx_bytes", 55);
  traffic_counters.Append(std::move(user_dict));

  SetFakeTrafficCounters(std::move(traffic_counters));
}

void FakeShillServiceClient::SetFakeTrafficCounters(
    base::Value::List fake_traffic_counters) {
  fake_traffic_counters_ = std::move(fake_traffic_counters);
}

void FakeShillServiceClient::SetTimeGetterForTest(
    base::RepeatingCallback<base::Time()> time_getter) {
  time_getter_ = std::move(time_getter);
}

}  // namespace ash
