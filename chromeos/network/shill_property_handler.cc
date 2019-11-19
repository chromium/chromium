// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_handler.h"

#include <stddef.h>

#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_state.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Limit the number of services or devices we observe. Since they are listed in
// priority order, it should be reasonable to ignore services past this.
const size_t kMaxObserved = 100;

const base::ListValue* GetListValue(const std::string& key,
                                    const base::Value& value) {
  const base::ListValue* vlist = NULL;
  if (!value.GetAsList(&vlist)) {
    LOG(ERROR) << "Error parsing key as list: " << key;
    return NULL;
  }
  return vlist;
}

}  // namespace

namespace chromeos {
namespace internal {

// Class to manage Shill service property changed observers. Observers are
// added on construction and removed on destruction. Runs the handler when
// OnPropertyChanged is called.
class ShillPropertyObserver : public ShillPropertyChangedObserver {
 public:
  typedef base::Callback<void(ManagedState::ManagedType type,
                              const std::string& service,
                              const std::string& name,
                              const base::Value& value)> Handler;

  ShillPropertyObserver(ManagedState::ManagedType type,
                        const std::string& path,
                        const Handler& handler)
      : type_(type), path_(path), handler_(handler) {
    if (type_ == ManagedState::MANAGED_TYPE_NETWORK) {
      DVLOG(2) << "ShillPropertyObserver: Network: " << path;
      ShillServiceClient::Get()->AddPropertyChangedObserver(
          dbus::ObjectPath(path_), this);
    } else if (type_ == ManagedState::MANAGED_TYPE_DEVICE) {
      DVLOG(2) << "ShillPropertyObserver: Device: " << path;
      ShillDeviceClient::Get()->AddPropertyChangedObserver(
          dbus::ObjectPath(path_), this);
    } else {
      NOTREACHED();
    }
  }

  ~ShillPropertyObserver() override {
    if (type_ == ManagedState::MANAGED_TYPE_NETWORK) {
      ShillServiceClient::Get()->RemovePropertyChangedObserver(
          dbus::ObjectPath(path_), this);
    } else if (type_ == ManagedState::MANAGED_TYPE_DEVICE) {
      ShillDeviceClient::Get()->RemovePropertyChangedObserver(
          dbus::ObjectPath(path_), this);
    } else {
      NOTREACHED();
    }
  }

  // ShillPropertyChangedObserver overrides.
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override {
    handler_.Run(type_, path_, key, value);
  }

 private:
  ManagedState::ManagedType type_;
  std::string path_;
  Handler handler_;

  DISALLOW_COPY_AND_ASSIGN(ShillPropertyObserver);
};

//------------------------------------------------------------------------------
// ShillPropertyHandler

ShillPropertyHandler::ShillPropertyHandler(Listener* listener)
    : listener_(listener), shill_manager_(ShillManagerClient::Get()) {}

ShillPropertyHandler::~ShillPropertyHandler() {
  // Delete network service observers.
  CHECK(shill_manager_ == ShillManagerClient::Get());
  shill_manager_->RemovePropertyChangedObserver(this);
}

void ShillPropertyHandler::Init() {
  UpdateManagerProperties();
  shill_manager_->AddPropertyChangedObserver(this);
}

void ShillPropertyHandler::UpdateManagerProperties() {
  NET_LOG(EVENT) << "UpdateManagerProperties";
  shill_manager_->GetProperties(base::Bind(
      &ShillPropertyHandler::ManagerPropertiesCallback, AsWeakPtr()));
}

bool ShillPropertyHandler::IsTechnologyAvailable(
    const std::string& technology) const {
  return available_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyEnabled(
    const std::string& technology) const {
  return enabled_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyEnabling(
    const std::string& technology) const {
  return enabling_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyDisabling(
    const std::string& technology) const {
  return disabling_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyProhibited(
    const std::string& technology) const {
  return prohibited_technologies_.count(technology) != 0;
}

bool ShillPropertyHandler::IsTechnologyUninitialized(
    const std::string& technology) const {
  return uninitialized_technologies_.count(technology) != 0;
}

void ShillPropertyHandler::SetTechnologyEnabled(
    const std::string& technology,
    bool enabled,
    const network_handler::ErrorCallback& error_callback) {
  if (enabled) {
    if (prohibited_technologies_.find(technology) !=
        prohibited_technologies_.end()) {
      chromeos::network_handler::RunErrorCallback(
          error_callback, "", "prohibited_technologies",
          "Ignored: Attempt to enable prohibited network technology " +
              technology);
      return;
    }
    enabling_technologies_.insert(technology);
    disabling_technologies_.erase(technology);
    shill_manager_->EnableTechnology(
        technology, base::DoNothing(),
        base::Bind(&ShillPropertyHandler::EnableTechnologyFailed, AsWeakPtr(),
                   technology, error_callback));
  } else {
    // Clear locally from enabling lists and add to the disabling list.
    enabling_technologies_.erase(technology);
    disabling_technologies_.insert(technology);
    shill_manager_->DisableTechnology(
        technology, base::DoNothing(),
        base::Bind(&ShillPropertyHandler::DisableTechnologyFailed, AsWeakPtr(),
                   technology, error_callback));
  }
}

void ShillPropertyHandler::SetProhibitedTechnologies(
    const std::vector<std::string>& prohibited_technologies,
    const network_handler::ErrorCallback& error_callback) {
  prohibited_technologies_.clear();
  prohibited_technologies_.insert(prohibited_technologies.begin(),
                                  prohibited_technologies.end());

  // Remove technologies from the other lists.
  // And manually disable them.
  for (const auto& technology : prohibited_technologies) {
    enabling_technologies_.erase(technology);
    enabled_technologies_.erase(technology);
    shill_manager_->DisableTechnology(
        technology, base::DoNothing(),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "DisableTechnology Failed", technology, error_callback));
  }

  // Send updated prohibited technology list to shill.
  const std::string prohibited_list =
      base::JoinString(prohibited_technologies, ",");
  base::Value value(prohibited_list);
  shill_manager_->SetProperty(
      "ProhibitedTechnologies", value, base::DoNothing(),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "SetTechnologiesProhibited Failed", prohibited_list,
                 error_callback));
}

void ShillPropertyHandler::SetCheckPortalList(
    const std::string& check_portal_list) {
  base::Value value(check_portal_list);
  shill_manager_->SetProperty(
      shill::kCheckPortalListProperty, value, base::DoNothing(),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "SetCheckPortalList Failed", "Manager",
                 network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetWakeOnLanEnabled(bool enabled) {
  base::Value value(enabled);
  shill_manager_->SetProperty(
      shill::kWakeOnLanEnabledProperty, value, base::DoNothing(),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "SetWakeOnLanEnabled Failed", "Manager",
                 network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetHostname(const std::string& hostname) {
  base::Value value(hostname);
  shill_manager_->SetProperty(
      shill::kDhcpPropertyHostnameProperty, value, base::DoNothing(),
      base::BindRepeating(&network_handler::ShillErrorCallbackFunction,
                          "SetHostname Failed", "Manager",
                          network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetNetworkThrottlingStatus(
    bool throttling_enabled,
    uint32_t upload_rate_kbits,
    uint32_t download_rate_kbits) {
  shill_manager_->SetNetworkThrottlingStatus(
      ShillManagerClient::NetworkThrottlingStatus{
          throttling_enabled, upload_rate_kbits, download_rate_kbits,
      },
      base::DoNothing(),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "SetNetworkThrottlingStatus failed", "Manager",
                 network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetFastTransitionStatus(bool enabled) {
  base::Value value(enabled);
  shill_manager_->SetProperty(
      shill::kWifiGlobalFTEnabledProperty, value, base::DoNothing(),
      base::BindRepeating(&network_handler::ShillErrorCallbackFunction,
                          "SetFastTransitionStatus failed", "Manager",
                          network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestScanByType(const std::string& type) const {
  shill_manager_->RequestScan(
      type, base::DoNothing(),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "RequestScan Failed", type, network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestProperties(ManagedState::ManagedType type,
                                             const std::string& path) {
  if (pending_updates_[type].find(path) != pending_updates_[type].end())
    return;  // Update already requested.

  NET_LOG(DEBUG) << "Request Properties: " << ManagedState::TypeToString(type)
                 << " For: " << path;
  pending_updates_[type].insert(path);
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    ShillServiceClient::Get()->GetProperties(
        dbus::ObjectPath(path),
        base::Bind(&ShillPropertyHandler::GetPropertiesCallback, AsWeakPtr(),
                   type, path));
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    ShillDeviceClient::Get()->GetProperties(
        dbus::ObjectPath(path),
        base::Bind(&ShillPropertyHandler::GetPropertiesCallback, AsWeakPtr(),
                   type, path));
  } else {
    NOTREACHED();
  }
}

void ShillPropertyHandler::OnPropertyChanged(const std::string& key,
                                             const base::Value& value) {
  ManagerPropertyChanged(key, value);
  CheckPendingStateListUpdates(key);
}

//------------------------------------------------------------------------------
// Private methods

void ShillPropertyHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    NET_LOG(ERROR) << "ManagerPropertiesCallback Failed: " << call_status;
    return;
  }
  NET_LOG(EVENT) << "ManagerPropertiesCallback: Success";
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
    ManagerPropertyChanged(iter.key(), iter.value());
  }

  CheckPendingStateListUpdates("");
}

void ShillPropertyHandler::CheckPendingStateListUpdates(
    const std::string& key) {
  // Once there are no pending updates, signal the state list changed callbacks.
  if ((key.empty() || key == shill::kServiceCompleteListProperty) &&
      pending_updates_[ManagedState::MANAGED_TYPE_NETWORK].size() == 0) {
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_NETWORK);
  }
  if ((key.empty() || key == shill::kDevicesProperty) &&
      pending_updates_[ManagedState::MANAGED_TYPE_DEVICE].size() == 0) {
    listener_->ManagedStateListChanged(ManagedState::MANAGED_TYPE_DEVICE);
  }
}

void ShillPropertyHandler::ManagerPropertyChanged(const std::string& key,
                                                  const base::Value& value) {
  if (key == shill::kDefaultServiceProperty) {
    std::string service_path;
    value.GetAsString(&service_path);
    NET_LOG(EVENT) << "Manager.DefaultService = " << service_path;
    listener_->DefaultNetworkServiceChanged(service_path);
    return;
  }
  NET_LOG(DEBUG) << "ManagerPropertyChanged: " << key;
  if (key == shill::kServiceCompleteListProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
      UpdateProperties(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
      UpdateObserved(ManagedState::MANAGED_TYPE_NETWORK, *vlist);
    }
  } else if (key == shill::kDevicesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
      UpdateProperties(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
      UpdateObserved(ManagedState::MANAGED_TYPE_DEVICE, *vlist);
    }
  } else if (key == shill::kAvailableTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateAvailableTechnologies(*vlist);
  } else if (key == shill::kEnabledTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateEnabledTechnologies(*vlist);
  } else if (key == shill::kUninitializedTechnologiesProperty) {
    const base::ListValue* vlist = GetListValue(key, value);
    if (vlist)
      UpdateUninitializedTechnologies(*vlist);
  } else if (key == shill::kProfilesProperty) {
    listener_->ProfileListChanged();
  } else if (key == shill::kCheckPortalListProperty) {
    std::string check_portal_list;
    if (value.GetAsString(&check_portal_list))
      listener_->CheckPortalListChanged(check_portal_list);
  } else {
    VLOG(2) << "Ignored Manager Property: " << key;
  }
}

void ShillPropertyHandler::UpdateProperties(ManagedState::ManagedType type,
                                            const base::ListValue& entries) {
  std::set<std::string>& requested_updates = requested_updates_[type];
  std::set<std::string> new_requested_updates;
  NET_LOG(DEBUG) << "UpdateProperties: " << ManagedState::TypeToString(type)
                 << ": " << entries.GetSize();
  for (base::ListValue::const_iterator iter = entries.begin();
       iter != entries.end(); ++iter) {
    std::string path;
    iter->GetAsString(&path);
    if (path.empty())
      continue;

    // We add a special case for devices here to work around an issue in shill
    // that prevents it from sending property changed signals for cellular
    // devices (see crbug.com/321854).
    if (type == ManagedState::MANAGED_TYPE_DEVICE ||
        requested_updates.find(path) == requested_updates.end()) {
      RequestProperties(type, path);
    }
    new_requested_updates.insert(path);
  }
  requested_updates.swap(new_requested_updates);
}

void ShillPropertyHandler::UpdateObserved(ManagedState::ManagedType type,
                                          const base::ListValue& entries) {
  ShillPropertyObserverMap& observer_map =
      (type == ManagedState::MANAGED_TYPE_NETWORK) ? observed_networks_
                                                   : observed_devices_;
  ShillPropertyObserverMap new_observed;
  for (const auto& entry : entries) {
    std::string path;
    entry.GetAsString(&path);
    if (path.empty())
      continue;
    auto iter = observer_map.find(path);
    std::unique_ptr<ShillPropertyObserver> observer;
    if (iter != observer_map.end()) {
      observer = std::move(iter->second);
    } else {
      // Create an observer for future updates.
      observer = std::make_unique<ShillPropertyObserver>(
          type, path, base::Bind(&ShillPropertyHandler::PropertyChangedCallback,
                                 AsWeakPtr()));
    }
    auto result =
        new_observed.insert(std::make_pair(path, std::move(observer)));
    if (!result.second) {
      LOG(ERROR) << path << " is duplicated in the list.";
    }
    observer_map.erase(path);
    // Limit the number of observed services.
    if (new_observed.size() >= kMaxObserved)
      break;
  }
  observer_map.swap(new_observed);
}

void ShillPropertyHandler::UpdateAvailableTechnologies(
    const base::ListValue& technologies) {
  NET_LOG(EVENT) << "AvailableTechnologies:" << technologies;
  std::set<std::string> new_available_technologies;
  for (const base::Value& technology : technologies.GetList())
    new_available_technologies.insert(technology.GetString());
  if (new_available_technologies == available_technologies_)
    return;
  available_technologies_.swap(new_available_technologies);
  // If any entries in |enabling_technologies_| are no longer available,
  // remove them from the enabling list.
  for (auto iter = enabling_technologies_.begin();
       iter != enabling_technologies_.end();) {
    if (!available_technologies_.count(*iter))
      iter = enabling_technologies_.erase(iter);
    else
      ++iter;
  }
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::UpdateEnabledTechnologies(
    const base::ListValue& technologies) {
  NET_LOG(EVENT) << "EnabledTechnologies:" << technologies;
  std::set<std::string> new_enabled_technologies;
  for (const base::Value& technology : technologies.GetList())
    new_enabled_technologies.insert(technology.GetString());
  if (new_enabled_technologies == enabled_technologies_)
    return;
  enabled_technologies_.swap(new_enabled_technologies);

  // If any entries in |disabling_technologies_| are disabled, remove them from
  // the disabling list.
  for (auto it = disabling_technologies_.begin();
       it != disabling_technologies_.end();) {
    base::Value technology_value(*it);
    if (!base::Contains(technologies.GetList(), technology_value))
      it = disabling_technologies_.erase(it);
    else
      ++it;
  }

  // If any entries in |enabling_technologies_| are enabled, remove them from
  // the enabling list.
  for (auto iter = enabling_technologies_.begin();
       iter != enabling_technologies_.end();) {
    if (enabled_technologies_.count(*iter))
      iter = enabling_technologies_.erase(iter);
    else
      ++iter;
  }
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::UpdateUninitializedTechnologies(
    const base::ListValue& technologies) {
  NET_LOG(EVENT) << "UninitializedTechnologies:" << technologies;
  std::set<std::string> new_uninitialized_technologies;
  for (const base::Value& technology : technologies.GetList())
    new_uninitialized_technologies.insert(technology.GetString());
  if (new_uninitialized_technologies == uninitialized_technologies_)
    return;
  uninitialized_technologies_.swap(new_uninitialized_technologies);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::EnableTechnologyFailed(
    const std::string& technology,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  enabling_technologies_.erase(technology);
  network_handler::ShillErrorCallbackFunction(
      "EnableTechnology Failed", technology, error_callback, dbus_error_name,
      dbus_error_message);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::DisableTechnologyFailed(
    const std::string& technology,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  disabling_technologies_.erase(technology);
  network_handler::ShillErrorCallbackFunction(
      "DisableTechnology Failed", technology, error_callback, dbus_error_name,
      dbus_error_message);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::GetPropertiesCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  pending_updates_[type].erase(path);
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // The shill service no longer exists.  This can happen when a network
    // has been removed.
    return;
  }
  NET_LOG(DEBUG) << "GetProperties received for "
                 << ManagedState::TypeToString(type) << ": " << path;
  listener_->UpdateManagedStateProperties(type, path, properties);

  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    // Request IPConfig properties.
    const base::Value* value;
    if (properties.GetWithoutPathExpansion(shill::kIPConfigProperty, &value))
      RequestIPConfig(type, path, *value);
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    // Clear and request IPConfig properties for each entry in IPConfigs.
    const base::Value* value;
    if (properties.GetWithoutPathExpansion(shill::kIPConfigsProperty, &value))
      RequestIPConfigsList(type, path, *value);
  }

  // Notify the listener only when all updates for that type have completed.
  if (pending_updates_[type].size() == 0)
    listener_->ManagedStateListChanged(type);
}

void ShillPropertyHandler::PropertyChangedCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& key,
    const base::Value& value) {
  if (type == ManagedState::MANAGED_TYPE_NETWORK &&
      key == shill::kIPConfigProperty) {
    RequestIPConfig(type, path, value);
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE &&
             key == shill::kIPConfigsProperty) {
    RequestIPConfigsList(type, path, value);
  }

  if (type == ManagedState::MANAGED_TYPE_NETWORK)
    listener_->UpdateNetworkServiceProperty(path, key, value);
  else if (type == ManagedState::MANAGED_TYPE_DEVICE)
    listener_->UpdateDeviceProperty(path, key, value);
  else
    NOTREACHED();
}

void ShillPropertyHandler::RequestIPConfig(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& ip_config_path_value) {
  std::string ip_config_path;
  if (!ip_config_path_value.GetAsString(&ip_config_path) ||
      ip_config_path.empty()) {
    NET_LOG(ERROR) << "Invalid IPConfig: " << path;
    return;
  }
  ShillIPConfigClient::Get()->GetProperties(
      dbus::ObjectPath(ip_config_path),
      base::Bind(&ShillPropertyHandler::GetIPConfigCallback, AsWeakPtr(), type,
                 path, ip_config_path));
}

void ShillPropertyHandler::RequestIPConfigsList(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& ip_config_list_value) {
  const base::ListValue* ip_configs;
  if (!ip_config_list_value.GetAsList(&ip_configs))
    return;
  for (base::ListValue::const_iterator iter = ip_configs->begin();
       iter != ip_configs->end(); ++iter) {
    RequestIPConfig(type, path, *iter);
  }
}

void ShillPropertyHandler::GetIPConfigCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& ip_config_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // IP Config properties not availabe. Shill will emit a property change
    // when they are.
    NET_LOG(EVENT) << "Failed to get IP Config properties: " << ip_config_path
                   << ": " << call_status << ", For: " << path;
    return;
  }
  NET_LOG(EVENT) << "IP Config properties received: " << path;
  listener_->UpdateIPConfigProperties(type, path, ip_config_path, properties);
}

}  // namespace internal
}  // namespace chromeos
