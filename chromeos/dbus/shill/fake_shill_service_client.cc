// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_shill_service_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void PassStubServiceProperties(
    const ShillServiceClient::DictionaryValueCallback& callback,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue* properties) {
  callback.Run(call_status, *properties);
}

void CallSortManagerServices() {
  ShillManagerClient::Get()->GetTestInterface()->SortManagerServices(true);
}

base::TimeDelta GetInteractiveDelay() {
  return ShillManagerClient::Get()->GetTestInterface()->GetInteractiveDelay();
}

// Extracts the hex SSID from shill |service_properties|.
std::string GetHexSSID(const base::Value& service_properties) {
  const std::string* hex_ssid =
      service_properties.FindStringKey(shill::kWifiHexSsid);
  if (hex_ssid)
    return *hex_ssid;
  const std::string* ssid =
      service_properties.FindStringKey(shill::kSSIDProperty);
  if (ssid)
    return base::HexEncode(ssid->c_str(), ssid->size());
  return std::string();
}

std::string GetSecurityClass(const base::Value& service_properties) {
  // Mimics shill's WiFiProvider::GetServiceParametersFromStorage with
  // WiFiService::ComputeSecurityClass  .
  const std::string* security_class =
      service_properties.FindStringKey(shill::kSecurityClassProperty);
  const std::string* security =
      service_properties.FindStringKey(shill::kSecurityProperty);
  if (security_class && security && *security_class != *security) {
    LOG(ERROR) << "Mismatch between SecurityClass " << *security_class
               << " and Security " << *security;
  }

  if (security_class)
    return *security_class;

  if (security) {
    if (*security == shill::kSecurityRsn || *security == shill::kSecurityWpa)
      return shill::kSecurityPsk;
    return *security;
  }

  return shill::kSecurityNone;
}

// Returns true if both |template_service_properties| and |service_properties|
// have the key |key| and both have the same value for it.
bool HaveSameValueForKey(const base::Value& template_service_properties,
                         const base::Value& service_properties,
                         base::StringPiece key) {
  const base::Value* template_service_value =
      template_service_properties.FindKey(key);
  const base::Value* service_value = service_properties.FindKey(key);
  return template_service_value && service_value &&
         *template_service_value == *service_value;
}

// Mimics shill's similar service matching logic. This is only invoked if
// |template_service_properties| and |service_properties| refer to a service of
// the same type.
bool IsSimilarService(const std::string& service_type,
                      const base::Value& template_service_properties,
                      const base::Value& service_properties) {
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
    shill::kProviderProperty,
    shill::kTetheringProperty};

}  // namespace

FakeShillServiceClient::FakeShillServiceClient() {}

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
    const DictionaryValueCallback& callback) {
  base::DictionaryValue* nested_dict = nullptr;
  std::unique_ptr<base::DictionaryValue> result_properties;
  DBusMethodCallStatus call_status;
  stub_services_.GetDictionaryWithoutPathExpansion(service_path.value(),
                                                   &nested_dict);
  if (nested_dict) {
    result_properties.reset(nested_dict->DeepCopy());
    // Remove credentials that Shill wouldn't send.
    result_properties->RemoveWithoutPathExpansion(shill::kPassphraseProperty,
                                                  nullptr);
    call_status = DBUS_METHOD_CALL_SUCCESS;
  } else {
    // This may happen if we remove services from the list.
    VLOG(2) << "Properties not found for: " << service_path.value();
    result_properties.reset(new base::DictionaryValue);
    call_status = DBUS_METHOD_CALL_FAILURE;
  }

  base::OnceClosure property_update =
      base::BindOnce(&PassStubServiceProperties, callback, call_status,
                     base::Owned(result_properties.release()));
  if (hold_back_service_property_updates_)
    recorded_property_updates_.push_back(std::move(property_update));
  else
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(property_update));
}

void FakeShillServiceClient::SetProperty(const dbus::ObjectPath& service_path,
                                         const std::string& name,
                                         const base::Value& value,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  if (!SetServiceProperty(service_path.value(), name, value)) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::SetProperties(
    const dbus::ObjectPath& service_path,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
    if (!SetServiceProperty(service_path.value(), iter.key(), iter.value())) {
      LOG(ERROR) << "Service not found: " << service_path.value();
      error_callback.Run("Error.InvalidService", "Invalid Service");
      return;
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::ClearProperty(
    const dbus::ObjectPath& service_path,
    const std::string& name,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* dict = nullptr;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(service_path.value(),
                                                        &dict)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  dict->RemoveWithoutPathExpansion(name, nullptr);
  // Note: Shill does not send notifications when properties are cleared.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::ClearProperties(
    const dbus::ObjectPath& service_path,
    const std::vector<std::string>& names,
    const ListValueCallback& callback,
    const ErrorCallback& error_callback) {
  base::Value* dict = stub_services_.FindKeyOfType(
      service_path.value(), base::Value::Type::DICTIONARY);
  if (!dict) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }

  base::ListValue result;
  for (const auto& name : names) {
    // Note: Shill does not send notifications when properties are cleared.
    result.AppendBoolean(dict->RemoveKey(name));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, std::move(result)));
}

void FakeShillServiceClient::Connect(const dbus::ObjectPath& service_path,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  VLOG(1) << "FakeShillServiceClient::Connect: " << service_path.value();
  base::DictionaryValue* service_properties = nullptr;
  if (!stub_services_.GetDictionary(service_path.value(),
                                    &service_properties)) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }

  // Set any other services of the same Type to 'offline' first, before setting
  // State to Association which will trigger sorting Manager.Services and
  // sending an update.
  SetOtherServicesOffline(service_path.value());

  // Clear Error.
  service_properties->SetKey(shill::kErrorProperty, base::Value(""));

  // Set Associating.
  base::Value associating_value(shill::kStateAssociation);
  SetServiceProperty(service_path.value(), shill::kStateProperty,
                     associating_value);

  // Stay Associating until the state is changed again after a delay.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillServiceClient::ContinueConnect,
                     weak_ptr_factory_.GetWeakPtr(), service_path.value()),
      GetInteractiveDelay());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::Disconnect(const dbus::ObjectPath& service_path,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  base::Value* service;
  if (!stub_services_.Get(service_path.value(), &service)) {
    error_callback.Run("Error.InvalidService", "Invalid Service");
    return;
  }
  // Set Idle after a delay
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillServiceClient::SetProperty,
                     weak_ptr_factory_.GetWeakPtr(), service_path,
                     shill::kStateProperty, base::Value(shill::kStateIdle),
                     base::DoNothing(), error_callback),
      GetInteractiveDelay());
  callback.Run();
}

void FakeShillServiceClient::Remove(const dbus::ObjectPath& service_path,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::ActivateCellularModem(
    const dbus::ObjectPath& service_path,
    const std::string& carrier,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* service_properties =
      GetModifiableServiceProperties(service_path.value(), false);
  if (!service_properties) {
    LOG(ERROR) << "Service not found: " << service_path.value();
    error_callback.Run("Error.InvalidService", "Invalid Service");
  }
  SetServiceProperty(service_path.value(), shill::kActivationStateProperty,
                     base::Value(shill::kActivationStateActivating));
  // Set Activated after a delay
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeShillServiceClient::SetCellularActivated,
                     weak_ptr_factory_.GetWeakPtr(), service_path,
                     error_callback),
      GetInteractiveDelay());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::CompleteCellularActivation(
    const dbus::ObjectPath& service_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillServiceClient::GetLoadableProfileEntries(
    const dbus::ObjectPath& service_path,
    const DictionaryValueCallback& callback) {
  ShillProfileClient::TestInterface* profile_client =
      ShillProfileClient::Get()->GetTestInterface();
  std::vector<std::string> profiles;
  profile_client->GetProfilePathsContainingService(service_path.value(),
                                                   &profiles);

  // Provide a dictionary with  {profile_path: service_path} entries for
  // profile_paths that contain the service.
  std::unique_ptr<base::DictionaryValue> result_properties(
      new base::DictionaryValue);
  for (const auto& profile : profiles) {
    result_properties->SetKey(profile, base::Value(service_path.value()));
  }

  DBusMethodCallStatus call_status = DBUS_METHOD_CALL_SUCCESS;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PassStubServiceProperties, callback, call_status,
                     base::Owned(result_properties.release())));
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
  base::DictionaryValue* properties =
      SetServiceProperties(service_path, guid, name, type, state, visible);

  if (!ipconfig_path.empty())
    properties->SetKey(shill::kIPConfigProperty, base::Value(ipconfig_path));

  ShillManagerClient::Get()->GetTestInterface()->AddManagerService(service_path,
                                                                   true);
}

base::DictionaryValue* FakeShillServiceClient::SetServiceProperties(
    const std::string& service_path,
    const std::string& guid,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    bool visible) {
  base::DictionaryValue* properties =
      GetModifiableServiceProperties(service_path, true);
  connect_behavior_.erase(service_path);

  // If |guid| is provided, set Service.GUID to that. Otherwise if a GUID is
  // stored in a profile entry, use that. Otherwise leave it blank. Shill does
  // not enforce a valid guid, we do that at the NetworkStateHandler layer.
  std::string guid_to_set = guid;
  if (guid_to_set.empty()) {
    std::string profile_path;
    base::DictionaryValue profile_properties;
    if (ShillProfileClient::Get()->GetTestInterface()->GetService(
            service_path, &profile_path, &profile_properties)) {
      profile_properties.GetStringWithoutPathExpansion(shill::kGuidProperty,
                                                       &guid_to_set);
    }
  }
  if (!guid_to_set.empty())
    properties->SetKey(shill::kGuidProperty, base::Value(guid_to_set));
  if (type == shill::kTypeWifi) {
    properties->SetKey(shill::kSSIDProperty, base::Value(name));
    properties->SetKey(shill::kWifiHexSsid,
                       base::Value(base::HexEncode(name.c_str(), name.size())));
  }
  properties->SetKey(shill::kNameProperty, base::Value(name));
  std::string device_path =
      ShillDeviceClient::Get()->GetTestInterface()->GetDevicePathForType(type);
  properties->SetKey(shill::kDeviceProperty, base::Value(device_path));
  properties->SetKey(shill::kTypeProperty, base::Value(type));
  properties->SetKey(shill::kStateProperty, base::Value(state));
  properties->SetKey(shill::kVisibleProperty, base::Value(visible));
  if (type == shill::kTypeWifi) {
    properties->SetKey(shill::kSecurityClassProperty,
                       base::Value(shill::kSecurityNone));
    properties->SetKey(shill::kModeProperty, base::Value(shill::kModeManaged));
  }

  // Ethernet is always connectable;
  if (type == shill::kTypeEthernet)
    properties->SetKey(shill::kConnectableProperty, base::Value(true));

  return properties;
}

void FakeShillServiceClient::RemoveService(const std::string& service_path) {
  stub_services_.RemoveWithoutPathExpansion(service_path, nullptr);
  connect_behavior_.erase(service_path);
  ShillManagerClient::Get()->GetTestInterface()->RemoveManagerService(
      service_path);
}

bool FakeShillServiceClient::SetServiceProperty(const std::string& service_path,
                                                const std::string& property,
                                                const base::Value& value) {
  base::DictionaryValue* dict = nullptr;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(service_path, &dict))
    return false;

  VLOG(1) << "Service.SetProperty: " << property << " = " << value
          << " For: " << service_path;

  base::DictionaryValue new_properties;
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
    if (base::StartsWith(property, "Provider.", case_sensitive))
      key = property.substr(strlen("Provider."));
    base::Value* provider = new_properties.SetKey(
        shill::kProviderProperty, base::Value(base::Value::Type::DICTIONARY));
    provider->SetKey(key, value.Clone());
    changed_property = shill::kProviderProperty;
  } else if (value.is_dict()) {
    const base::DictionaryValue* new_dict = nullptr;
    value.GetAsDictionary(&new_dict);
    CHECK(new_dict);
    std::unique_ptr<base::Value> cur_value;
    base::DictionaryValue* cur_dict;
    if (dict->RemoveWithoutPathExpansion(property, &cur_value) &&
        cur_value->GetAsDictionary(&cur_dict)) {
      cur_dict->Clear();
      cur_dict->MergeDictionary(new_dict);
      new_properties.SetWithoutPathExpansion(property, std::move(cur_value));
    } else {
      new_properties.SetKey(property, value.Clone());
    }
    changed_property = property;
  } else {
    new_properties.SetKey(property, value.Clone());
    changed_property = property;
  }

  // Make PSK networks connectable if 'Passphrase' is set.
  if (changed_property == shill::kPassphraseProperty && value.is_string() &&
      !value.GetString().empty()) {
    new_properties.SetKey(shill::kPassphraseRequiredProperty,
                          base::Value(false));
    base::Value* security = dict->FindKey(shill::kSecurityClassProperty);
    if (security && security->is_string() &&
        security->GetString() == shill::kSecurityPsk) {
      new_properties.SetKey(shill::kConnectableProperty, base::Value(true));
    }
  }

  dict->MergeDictionary(&new_properties);

  // Add or update the profile entry.
  ShillProfileClient::TestInterface* profile_test =
      ShillProfileClient::Get()->GetTestInterface();
  if (property == shill::kProfileProperty) {
    std::string profile_path;
    if (value.GetAsString(&profile_path)) {
      if (!profile_path.empty())
        profile_test->AddService(profile_path, service_path);
    } else {
      LOG(ERROR) << "Profile value is not a String!";
    }
  } else {
    std::string profile_path;
    if (dict->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                            &profile_path) &&
        !profile_path.empty()) {
      profile_test->UpdateService(profile_path, service_path);
    }
  }

  // Notify the Manager if the state changed (affects DefaultService).
  if (property == shill::kStateProperty) {
    std::string state;
    value.GetAsString(&state);
    ShillManagerClient::Get()->GetTestInterface()->ServiceStateChanged(
        service_path, state);
  }

  // If the State or Visibility changes, the sort order of service lists may
  // change and the DefaultService property may change.
  if (property == shill::kStateProperty ||
      property == shill::kVisibleProperty) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CallSortManagerServices));
  }

  // Notifiy Chrome of the property change.
  base::OnceClosure property_update =
      base::BindOnce(&FakeShillServiceClient::NotifyObserversPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     dbus::ObjectPath(service_path), changed_property);
  if (hold_back_service_property_updates_)
    recorded_property_updates_.push_back(std::move(property_update));
  else
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(property_update));
  return true;
}

const base::DictionaryValue* FakeShillServiceClient::GetServiceProperties(
    const std::string& service_path) const {
  const base::DictionaryValue* properties = nullptr;
  stub_services_.GetDictionaryWithoutPathExpansion(service_path, &properties);
  return properties;
}

bool FakeShillServiceClient::ClearConfiguredServiceProperties(
    const std::string& service_path) {
  base::Value* service_dict = GetModifiableServiceProperties(
      service_path, false /* create_if_missing */);
  if (!service_dict)
    return false;

  const base::Value* visible_property = service_dict->FindKeyOfType(
      shill::kVisibleProperty, base::Value::Type::BOOLEAN);
  if (!visible_property || !visible_property->GetBool()) {
    stub_services_.RemoveKey(service_path);
    RemoveService(service_path);
    return true;
  }

  base::DictionaryValue properties_after_delete_entry;

  // Explicitly clear the profile property using SetServiceProperty so a
  // notification is sent about that.
  SetServiceProperty(service_path, shill::kProfileProperty,
                     base::Value(std::string()));
  properties_after_delete_entry.SetKey(shill::kProfileProperty,
                                       base::Value(std::string()));

  for (const std::string& property_to_retain : kIntrinsicServiceProperties) {
    const base::Value* value = service_dict->FindKey(property_to_retain);
    if (!value)
      continue;
    properties_after_delete_entry.SetKey(property_to_retain, value->Clone());
  }
  stub_services_.SetKey(service_path, std::move(properties_after_delete_entry));
  return true;
}

std::string FakeShillServiceClient::FindServiceMatchingGUID(
    const std::string& guid) {
  for (const auto& service_pair : stub_services_.DictItems()) {
    const auto& service_path = service_pair.first;
    const auto& service_properties = service_pair.second;

    const std::string* service_guid =
        service_properties.FindStringKey(shill::kGuidProperty);
    if (service_guid && *service_guid == guid)
      return service_path;
  }

  return std::string();
}

std::string FakeShillServiceClient::FindSimilarService(
    const base::Value& template_service_properties) {
  const std::string* template_type =
      template_service_properties.FindStringKey(shill::kTypeProperty);
  if (!template_type)
    return std::string();

  for (const auto& service_pair : stub_services_.DictItems()) {
    const auto& service_path = service_pair.first;
    const auto& service_properties = service_pair.second;

    const std::string* service_type =
        service_properties.FindStringKey(shill::kTypeProperty);
    if (!service_type || *service_type != *template_type)
      continue;

    if (IsSimilarService(*service_type, template_service_properties,
                         service_properties)) {
      return service_path;
    }
  }

  return std::string();
}

void FakeShillServiceClient::ClearServices() {
  ShillManagerClient::Get()->GetTestInterface()->ClearManagerServices();
  stub_services_.Clear();
  connect_behavior_.clear();
}

void FakeShillServiceClient::SetConnectBehavior(const std::string& service_path,
                                                const base::Closure& behavior) {
  connect_behavior_[service_path] = behavior;
}

void FakeShillServiceClient::SetHoldBackServicePropertyUpdates(bool hold_back) {
  hold_back_service_property_updates_ = hold_back;
  std::vector<base::OnceClosure> property_updates;
  recorded_property_updates_.swap(property_updates);

  if (hold_back_service_property_updates_)
    return;

  for (auto& property_update : property_updates)
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(property_update));
}

void FakeShillServiceClient::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& service_path,
    const std::string& property) {
  base::DictionaryValue* dict = nullptr;
  std::string path = service_path.value();
  if (!stub_services_.GetDictionaryWithoutPathExpansion(path, &dict)) {
    LOG(ERROR) << "Notify for unknown service: " << path;
    return;
  }
  base::Value* value = nullptr;
  if (!dict->GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: " << path << " : " << property;
    return;
  }
  for (auto& observer : GetObserverList(service_path))
    observer.OnPropertyChanged(property, *value);
}

base::DictionaryValue* FakeShillServiceClient::GetModifiableServiceProperties(
    const std::string& service_path,
    bool create_if_missing) {
  base::DictionaryValue* properties = nullptr;
  if (!stub_services_.GetDictionaryWithoutPathExpansion(service_path,
                                                        &properties) &&
      create_if_missing) {
    properties = stub_services_.SetDictionary(
        service_path, std::make_unique<base::DictionaryValue>());
  }
  return properties;
}

FakeShillServiceClient::PropertyObserverList&
FakeShillServiceClient::GetObserverList(const dbus::ObjectPath& device_path) {
  auto iter = observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = base::WrapUnique(observer_list);
  return *observer_list;
}

void FakeShillServiceClient::SetOtherServicesOffline(
    const std::string& service_path) {
  const base::DictionaryValue* service_properties =
      GetServiceProperties(service_path);
  if (!service_properties) {
    LOG(ERROR) << "Missing service: " << service_path;
    return;
  }
  std::string service_type;
  service_properties->GetString(shill::kTypeProperty, &service_type);
  // Set all other services of the same type to offline (Idle).
  for (base::DictionaryValue::Iterator iter(stub_services_); !iter.IsAtEnd();
       iter.Advance()) {
    std::string path = iter.key();
    if (path == service_path)
      continue;
    base::DictionaryValue* properties;
    if (!stub_services_.GetDictionaryWithoutPathExpansion(path, &properties))
      NOTREACHED();

    std::string type;
    properties->GetString(shill::kTypeProperty, &type);
    if (type != service_type)
      continue;
    properties->SetKey(shill::kStateProperty, base::Value(shill::kStateIdle));
  }
}

void FakeShillServiceClient::SetCellularActivated(
    const dbus::ObjectPath& service_path,
    const ErrorCallback& error_callback) {
  SetProperty(service_path, shill::kActivationStateProperty,
              base::Value(shill::kActivationStateActivated), base::DoNothing(),
              error_callback);
  SetProperty(service_path, shill::kConnectableProperty, base::Value(true),
              base::DoNothing(), error_callback);
}

void FakeShillServiceClient::ContinueConnect(const std::string& service_path) {
  VLOG(1) << "FakeShillServiceClient::ContinueConnect: " << service_path;
  base::DictionaryValue* service_properties = nullptr;
  if (!stub_services_.GetDictionary(service_path, &service_properties)) {
    LOG(ERROR) << "Service not found: " << service_path;
    return;
  }

  if (base::Contains(connect_behavior_, service_path)) {
    const base::Closure& custom_connect_behavior =
        connect_behavior_[service_path];
    VLOG(1) << "Running custom connect behavior for " << service_path;
    custom_connect_behavior.Run();
    return;
  }

  // No custom connect behavior set, continue with the default connect behavior.
  std::string passphrase;
  service_properties->GetStringWithoutPathExpansion(shill::kPassphraseProperty,
                                                    &passphrase);
  if (passphrase == "failure") {
    // Simulate a password failure.
    SetServiceProperty(service_path, shill::kErrorProperty,
                       base::Value(shill::kErrorBadPassphrase));
    SetServiceProperty(service_path, shill::kStateProperty,
                       base::Value(shill::kStateFailure));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&FakeShillServiceClient::SetServiceProperty),
            weak_ptr_factory_.GetWeakPtr(), service_path, shill::kErrorProperty,
            base::Value(shill::kErrorBadPassphrase)));
  } else {
    // Set Online.
    VLOG(1) << "Setting state to Online " << service_path;
    SetServiceProperty(service_path, shill::kStateProperty,
                       base::Value(shill::kStateOnline));
  }
}

}  // namespace chromeos
