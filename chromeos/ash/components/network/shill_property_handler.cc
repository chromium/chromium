// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/shill_property_handler.h"

#include <stddef.h>

#include <memory>
#include <sstream>

#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Limit the number of services or devices we observe. Since they are listed in
// priority order, it should be reasonable to ignore services past this.
const size_t kMaxObserved = 100;

bool CheckListValue(const std::string& key, const base::Value& value) {
  if (!value.is_list()) {
    NET_LOG(ERROR) << "Error parsing key as list: " << key;
    return false;
  }
  return true;
}

}  // namespace

namespace ash::internal {

// Class to manage Shill service property changed observers. Observers are
// added on construction and removed on destruction. Runs the handler when
// OnPropertyChanged is called.
class ShillPropertyObserver : public ShillPropertyChangedObserver {
 public:
  using Handler = base::RepeatingCallback<void(ManagedState::ManagedType type,
                                               const std::string& service,
                                               const std::string& name,
                                               const base::Value& value)>;

  ShillPropertyObserver(ManagedState::ManagedType type,
                        const std::string& path,
                        const Handler& handler)
      : type_(type), path_(path), handler_(handler) {
    switch (type_) {
      case ManagedState::MANAGED_TYPE_NETWORK:
        DVLOG(2) << "ShillPropertyObserver: Network: " << path;
        ShillServiceClient::Get()->AddPropertyChangedObserver(
            dbus::ObjectPath(path_), this);
        break;
      case ManagedState::MANAGED_TYPE_DEVICE:
        DVLOG(2) << "ShillPropertyObserver: Device: " << path;
        ShillDeviceClient::Get()->AddPropertyChangedObserver(
            dbus::ObjectPath(path_), this);
        break;
    }
  }

  ShillPropertyObserver(const ShillPropertyObserver&) = delete;
  ShillPropertyObserver& operator=(const ShillPropertyObserver&) = delete;

  ~ShillPropertyObserver() override {
    switch (type_) {
      case ManagedState::MANAGED_TYPE_NETWORK:
        ShillServiceClient::Get()->RemovePropertyChangedObserver(
            dbus::ObjectPath(path_), this);
        break;
      case ManagedState::MANAGED_TYPE_DEVICE:
        ShillDeviceClient::Get()->RemovePropertyChangedObserver(
            dbus::ObjectPath(path_), this);
        break;
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
  shill_manager_->GetProperties(
      base::BindOnce(&ShillPropertyHandler::ManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
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
    network_handler::ErrorCallback error_callback,
    base::OnceClosure success_callback) {
  if (enabled) {
    if (base::Contains(prohibited_technologies_, technology)) {
      NET_LOG(ERROR) << "Attempt to enable prohibited network technology: "
                     << technology;
      network_handler::RunErrorCallback(std::move(error_callback),
                                        "prohibited_technologies");
      NetworkMetricsHelper::LogEnableTechnologyResult(technology,
                                                      /*success=*/false);
      return;
    }
    enabling_technologies_.insert(technology);
    disabling_technologies_.erase(technology);
    shill_manager_->EnableTechnology(
        technology,
        base::BindOnce(&ShillPropertyHandler::EnableTechnologySuccess,
                       weak_ptr_factory_.GetWeakPtr(), technology,
                       std::move(success_callback)),
        base::BindOnce(&ShillPropertyHandler::EnableTechnologyFailed,
                       weak_ptr_factory_.GetWeakPtr(), technology,
                       std::move(error_callback)));
  } else {
    // Clear locally from enabling lists and add to the disabling list.
    enabling_technologies_.erase(technology);
    disabling_technologies_.insert(technology);
    shill_manager_->DisableTechnology(
        technology,
        base::BindOnce(&ShillPropertyHandler::DisableTechnologySuccess,
                       weak_ptr_factory_.GetWeakPtr(), technology,
                       std::move(success_callback)),
        base::BindOnce(&ShillPropertyHandler::DisableTechnologyFailed,
                       weak_ptr_factory_.GetWeakPtr(), technology,
                       std::move(error_callback)));
  }
}

void ShillPropertyHandler::SetProhibitedTechnologies(
    const std::vector<std::string>& prohibited_technologies) {
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
        base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                       "DisableTechnology Failed", technology,
                       network_handler::ErrorCallback()));
  }

  // Send updated prohibited technology list to shill.
  const std::string prohibited_list =
      base::JoinString(prohibited_technologies, ",");
  base::Value value(prohibited_list);
  shill_manager_->SetProperty(
      "ProhibitedTechnologies", value, base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "SetTechnologiesProhibited Failed", prohibited_list,
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetWakeOnLanEnabled(bool enabled) {
  base::Value value(enabled);
  shill_manager_->SetProperty(
      shill::kWakeOnLanEnabledProperty, value, base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "SetWakeOnLanEnabled Failed", "Manager",
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetHostname(const std::string& hostname) {
  base::Value value(hostname);
  shill_manager_->SetProperty(
      shill::kDhcpPropertyHostnameProperty, value, base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "SetHostname Failed", "Manager",
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetNetworkThrottlingStatus(
    bool throttling_enabled,
    uint32_t upload_rate_kbits,
    uint32_t download_rate_kbits) {
  shill_manager_->SetNetworkThrottlingStatus(
      ShillManagerClient::NetworkThrottlingStatus{
          throttling_enabled,
          upload_rate_kbits,
          download_rate_kbits,
      },
      base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "SetNetworkThrottlingStatus failed", "Manager",
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::SetFastTransitionStatus(bool enabled) {
  base::Value value(enabled);
  shill_manager_->SetProperty(
      shill::kWifiGlobalFTEnabledProperty, value, base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "SetFastTransitionStatus failed", "Manager",
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestScanByType(const std::string& type) const {
  shill_manager_->RequestScan(
      type, base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "RequestScan Failed", type,
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::RequestProperties(ManagedState::ManagedType type,
                                             const std::string& path) {
  if (base::Contains(pending_updates_[type], path)) {
    return;  // Update already requested.
  }

  NET_LOG(DEBUG) << "Request Properties for: " << NetworkPathId(path);
  pending_updates_[type].insert(path);
  switch (type) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      ShillServiceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillPropertyHandler::GetPropertiesCallback,
                         weak_ptr_factory_.GetWeakPtr(), type, path));
      return;
    case ManagedState::MANAGED_TYPE_DEVICE:
      ShillDeviceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillPropertyHandler::GetPropertiesCallback,
                         weak_ptr_factory_.GetWeakPtr(), type, path));
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void ShillPropertyHandler::RequestPortalDetection(
    const std::string& service_path) {
  ShillServiceClient::Get()->RequestPortalDetection(
      dbus::ObjectPath(service_path),
      base::BindOnce(
          [](const std::string& service_path, bool success) {
            if (!success) {
              NET_LOG(ERROR) << "Shill RecheckPortal call failed for: "
                             << NetworkPathId(service_path);
            }
          },
          service_path));
}

void ShillPropertyHandler::RequestTrafficCounters(
    const std::string& service_path,
    chromeos::DBusMethodCallback<base::Value> callback) {
  ShillServiceClient::Get()->RequestTrafficCounters(
      dbus::ObjectPath(service_path),
      base::BindOnce(
          [](const std::string& service_path,
             chromeos::DBusMethodCallback<base::Value> callback,
             std::optional<base::Value> traffic_counters) {
            if (!traffic_counters) {
              NET_LOG(ERROR) << "Error requesting traffic counters for: "
                             << NetworkPathId(service_path);
            } else {
              NET_LOG(EVENT) << "Received traffic counters for "
                             << NetworkPathId(service_path);
            }
            std::move(callback).Run(std::move(traffic_counters));
          },
          service_path, std::move(callback)));
}

void ShillPropertyHandler::ResetTrafficCounters(
    const std::string& service_path) {
  NET_LOG(EVENT) << "ResetTrafficCounters: Success";

  ShillServiceClient::Get()->ResetTrafficCounters(
      dbus::ObjectPath(service_path), base::DoNothing(),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "ResetTrafficCounters Failed", service_path,
                     network_handler::ErrorCallback()));
}

void ShillPropertyHandler::OnPropertyChanged(const std::string& key,
                                             const base::Value& value) {
  ManagerPropertyChanged(key, value);
  CheckPendingStateListUpdates(key);
}

//------------------------------------------------------------------------------
// Private methods

void ShillPropertyHandler::ManagerPropertiesCallback(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "ManagerPropertiesCallback Failed";
    return;
  }
  NET_LOG(EVENT) << "ManagerPropertiesCallback: Success";
  for (const auto item : *properties) {
    ManagerPropertyChanged(item.first, item.second);
  }

  CheckPendingStateListUpdates("");
}

void ShillPropertyHandler::CheckPendingStateListUpdates(
    const std::string& key) {
  // Once there are no pending updates, signal the state list changed
  // callbacks.
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
    if (value.is_string()) {
      service_path = value.GetString();
    }
    NET_LOG(EVENT) << "Manager.DefaultService = "
                   << NetworkPathId(service_path);
    listener_->DefaultNetworkServiceChanged(service_path);
    return;
  }
  NET_LOG(DEBUG) << "ManagerPropertyChanged: " << key << " = " << value;
  if (key == shill::kServiceCompleteListProperty) {
    if (CheckListValue(key, value)) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_NETWORK,
                                   value.GetList());
      UpdateProperties(ManagedState::MANAGED_TYPE_NETWORK, value);
      UpdateObserved(ManagedState::MANAGED_TYPE_NETWORK, value);
    }
  } else if (key == shill::kDevicesProperty) {
    if (CheckListValue(key, value)) {
      listener_->UpdateManagedList(ManagedState::MANAGED_TYPE_DEVICE,
                                   value.GetList());
      UpdateProperties(ManagedState::MANAGED_TYPE_DEVICE, value);
      UpdateObserved(ManagedState::MANAGED_TYPE_DEVICE, value);
    }
  } else if (key == shill::kAvailableTechnologiesProperty) {
    if (CheckListValue(key, value)) {
      UpdateAvailableTechnologies(value);
    }
  } else if (key == shill::kEnabledTechnologiesProperty) {
    if (CheckListValue(key, value)) {
      UpdateEnabledTechnologies(value);
    }
  } else if (key == shill::kUninitializedTechnologiesProperty) {
    if (CheckListValue(key, value)) {
      UpdateUninitializedTechnologies(value);
    }
  } else if (key == shill::kProhibitedTechnologiesProperty) {
    if (value.is_string()) {
      UpdateProhibitedTechnologies(value.GetString());
    }
  } else if (key == shill::kProfilesProperty) {
    if (value.is_list()) {
      listener_->ProfileListChanged(value.GetList());
    }
  } else if (key == shill::kCheckPortalListProperty) {
    if (value.is_string()) {
      listener_->CheckPortalListChanged(value.GetString());
    }
  } else if (key == shill::kDhcpPropertyHostnameProperty) {
    if (value.is_string()) {
      listener_->HostnameChanged(value.GetString());
    }
  } else {
    VLOG(2) << "Ignored Manager Property: " << key;
  }
}

void ShillPropertyHandler::UpdateProperties(ManagedState::ManagedType type,
                                            const base::Value& entries) {
  std::set<std::string>& requested_updates = requested_updates_[type];
  std::set<std::string> new_requested_updates;
  NET_LOG(DEBUG) << "UpdateProperties: " << ManagedState::TypeToString(type)
                 << ": " << entries.GetList().size();
  for (const auto& entry : entries.GetList()) {
    const std::string* path = entry.GetIfString();
    if (!path || (*path).empty())
      continue;

    // We add a special case for devices here to work around an issue in shill
    // that prevents it from sending property changed signals for cellular
    // devices (see crbug.com/321854).
    if (type == ManagedState::MANAGED_TYPE_DEVICE ||
        !base::Contains(requested_updates, *path)) {
      RequestProperties(type, *path);
    }
    new_requested_updates.insert(*path);
  }
  requested_updates.swap(new_requested_updates);
}

void ShillPropertyHandler::UpdateObserved(ManagedState::ManagedType type,
                                          const base::Value& entries) {
  ShillPropertyObserverMap& observer_map =
      (type == ManagedState::MANAGED_TYPE_NETWORK) ? observed_networks_
                                                   : observed_devices_;
  ShillPropertyObserverMap new_observed;
  for (const auto& entry : entries.GetList()) {
    const std::string* path = entry.GetIfString();
    if (!path || (*path).empty())
      continue;
    auto iter = observer_map.find(*path);
    std::unique_ptr<ShillPropertyObserver> observer;
    if (iter != observer_map.end()) {
      observer = std::move(iter->second);
    } else {
      // Create an observer for future updates.
      observer = std::make_unique<ShillPropertyObserver>(
          type, *path,
          base::BindRepeating(&ShillPropertyHandler::PropertyChangedCallback,
                              weak_ptr_factory_.GetWeakPtr()));
    }
    auto result =
        new_observed.insert(std::make_pair(*path, std::move(observer)));
    if (!result.second) {
      NET_LOG(ERROR) << *path << " is duplicated in the list.";
    }
    observer_map.erase(*path);
    // Limit the number of observed services.
    if (new_observed.size() >= kMaxObserved)
      break;
  }
  observer_map.swap(new_observed);
}

void ShillPropertyHandler::UpdateAvailableTechnologies(
    const base::Value& technologies) {
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
    const base::Value& technologies) {
  NET_LOG(EVENT) << "EnabledTechnologies:" << technologies;
  std::set<std::string> new_enabled_technologies;
  for (const base::Value& technology : technologies.GetList())
    new_enabled_technologies.insert(technology.GetString());
  if (new_enabled_technologies == enabled_technologies_)
    return;
  enabled_technologies_.swap(new_enabled_technologies);

  // If any entries in |disabling_technologies_| are disabled, remove them
  // from the disabling list.
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
    const base::Value& technologies) {
  NET_LOG(EVENT) << "UninitializedTechnologies:" << technologies;
  std::set<std::string> new_uninitialized_technologies;
  for (const base::Value& technology : technologies.GetList())
    new_uninitialized_technologies.insert(technology.GetString());
  if (new_uninitialized_technologies == uninitialized_technologies_)
    return;
  uninitialized_technologies_.swap(new_uninitialized_technologies);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::UpdateProhibitedTechnologies(
    const std::string& technologies) {
  std::vector<std::string> prohibited_list = base::SplitString(
      technologies, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::set<std::string> new_prohibited_technologies(prohibited_list.begin(),
                                                    prohibited_list.end());
  if (new_prohibited_technologies == prohibited_technologies_)
    return;
  prohibited_technologies_.swap(new_prohibited_technologies);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::EnableTechnologySuccess(
    const std::string& technology,
    base::OnceClosure success_callback) {
  NetworkMetricsHelper::LogEnableTechnologyResult(technology, /*success=*/true);
  std::move(success_callback).Run();
}

void ShillPropertyHandler::EnableTechnologyFailed(
    const std::string& technology,
    network_handler::ErrorCallback error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  NetworkMetricsHelper::LogEnableTechnologyResult(technology,
                                                  /*success=*/false,
                                                  dbus_error_name);
  enabling_technologies_.erase(technology);
  network_handler::ShillErrorCallbackFunction(
      "EnableTechnology Failed", technology, std::move(error_callback),
      dbus_error_name, dbus_error_message);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::DisableTechnologySuccess(
    const std::string& technology,
    base::OnceClosure success_callback) {
  NetworkMetricsHelper::LogDisableTechnologyResult(technology,
                                                   /*success=*/true);
  std::move(success_callback).Run();
}

void ShillPropertyHandler::DisableTechnologyFailed(
    const std::string& technology,
    network_handler::ErrorCallback error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  NetworkMetricsHelper::LogDisableTechnologyResult(technology,
                                                   /*success=*/false,
                                                   dbus_error_name);
  disabling_technologies_.erase(technology);
  network_handler::ShillErrorCallbackFunction(
      "DisableTechnology Failed", technology, std::move(error_callback),
      dbus_error_name, dbus_error_message);
  listener_->TechnologyListChanged();
}

void ShillPropertyHandler::GetPropertiesCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    std::optional<base::Value::Dict> properties) {
  pending_updates_[type].erase(path);
  if (!properties) {
    // The shill service no longer exists.  This can happen when a network
    // has been removed.
    return;
  }
  NET_LOG(DEBUG) << "GetProperties received for " << NetworkPathId(path);
  listener_->UpdateManagedStateProperties(type, path, *properties);

  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    // Request IPConfig properties.
    const base::Value* value = properties->Find(shill::kIPConfigProperty);
    if (value)
      RequestIPConfig(type, path, *value);
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    // Clear and request IPConfig properties for each entry in IPConfigs.
    const base::Value* value = properties->Find(shill::kIPConfigsProperty);
    if (value)
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

  switch (type) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      listener_->UpdateNetworkServiceProperty(path, key, value);
      return;
    case ManagedState::MANAGED_TYPE_DEVICE:
      listener_->UpdateDeviceProperty(path, key, value);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void ShillPropertyHandler::RequestIPConfig(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& ip_config_path_value) {
  const std::string* ip_config_path = ip_config_path_value.GetIfString();
  if (!ip_config_path || (*ip_config_path).empty()) {
    NET_LOG(ERROR) << "Invalid IPConfig: " << path;
    return;
  }
  ShillIPConfigClient::Get()->GetProperties(
      dbus::ObjectPath(*ip_config_path),
      base::BindOnce(&ShillPropertyHandler::GetIPConfigCallback,
                     weak_ptr_factory_.GetWeakPtr(), type, path,
                     *ip_config_path));
}

void ShillPropertyHandler::RequestIPConfigsList(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& ip_config_list_value) {
  if (!ip_config_list_value.is_list())
    return;
  for (const auto& entry : ip_config_list_value.GetList()) {
    RequestIPConfig(type, path, entry);
  }
}

void ShillPropertyHandler::GetIPConfigCallback(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& ip_config_path,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    // IP Config properties not available. Shill will emit a property change
    // when they are.
    NET_LOG(EVENT) << "Failed to get IP Config properties: " << ip_config_path
                   << ", For: " << NetworkPathId(path);
    return;
  }
  NET_LOG(EVENT) << "IP Config properties received: " << NetworkPathId(path);
  listener_->UpdateIPConfigProperties(type, path, ip_config_path,
                                      std::move(*properties));
}

}  // namespace ash::internal
