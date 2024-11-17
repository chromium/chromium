// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_configuration_handler.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Strip surrounding "" from keys (if present).
std::string StripQuotations(const std::string& in_str) {
  size_t len = in_str.length();
  if (len >= 2 && in_str[0] == '"' && in_str[len - 1] == '"') {
    return in_str.substr(1, len - 2);
  }
  return in_str;
}

void InvokeErrorCallback(const std::string& service_path,
                         network_handler::ErrorCallback error_callback,
                         const std::string& error_name) {
  NET_LOG(ERROR) << "Config Error: " << error_name
                 << " For: " << NetworkPathId(service_path);
  network_handler::RunErrorCallback(std::move(error_callback), error_name);
}

void SetNetworkProfileErrorCallback(
    const std::string& service_path,
    const std::string& profile_path,
    network_handler::ErrorCallback error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.SetNetworkProfile Failed: " + profile_path, service_path,
      std::move(error_callback), dbus_error_name, dbus_error_message);
}

void ManagerSetPropertiesErrorCallback(const std::string& dbus_error_name,
                                       const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "ShillManagerClient.SetProperties Failed", std::string(),
      base::NullCallback(), dbus_error_name, dbus_error_message);
}

void LogConfigProperties(const std::string& desc,
                         const std::string& path,
                         const base::Value::Dict& properties) {
  for (auto iter : properties) {
    std::string v = "******";
    if (shill_property_util::IsLoggableShillProperty(iter.first)) {
      base::JSONWriter::Write(iter.second, &v);
    }
    NET_LOG(USER) << desc << ": " << path + "." + iter.first + "=" + v;
  }
}

// Returns recognized dbus error names or |default_error_name|.
// TODO(stevenjb): Expand this list and update
// network_element::AddErrorLocalizedStrings.
std::string GetErrorName(const std::string& dbus_error_name,
                         const std::string& dbus_error_message,
                         const std::string& default_error_name) {
  if (dbus_error_name == shill::kErrorResultInvalidPassphrase) {
    return dbus_error_name;
  }
  // TODO(b/365490226): do not rely on the `dbus_error_message` and return newly
  // created `dbus_error_name`.
  if ((dbus_error_name == shill::kErrorResultNotFound) &&
      (dbus_error_message == kTemporaryServiceConfiguredButNotUsable)) {
    return base::StrCat({"Config.CreateConfiguration ",
                         kTemporaryServiceConfiguredButNotUsable});
  }
  return default_error_name;
}

std::string GetString(const base::Value::Dict& dict, const char* key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

}  // namespace

// Helper class to request from Shill the profile entries associated with a
// Service and delete the service from each profile. Triggers either
// |callback| on success or |error_callback| on failure, and calls
// |handler|->ProfileEntryDeleterCompleted() on completion to delete itself.
class NetworkConfigurationHandler::ProfileEntryDeleter {
 public:
  ProfileEntryDeleter(NetworkConfigurationHandler* handler,
                      const std::string& service_path,
                      const std::string& guid,
                      std::optional<RemoveConfirmer> remove_confirmer,
                      base::OnceClosure callback,
                      network_handler::ErrorCallback error_callback)
      : owner_(handler),
        service_path_(service_path),
        guid_(guid),
        remove_confirmer_(std::move(remove_confirmer)),
        callback_(std::move(callback)),
        error_callback_(std::move(error_callback)) {}

  ProfileEntryDeleter(const ProfileEntryDeleter&) = delete;
  ProfileEntryDeleter& operator=(const ProfileEntryDeleter&) = delete;

  void RestrictToProfilePath(const std::string& profile_path) {
    restrict_to_profile_path_ = profile_path;
  }

  void Run() {
    ShillServiceClient::Get()->GetLoadableProfileEntries(
        dbus::ObjectPath(service_path_),
        base::BindOnce(&ProfileEntryDeleter::GetProfileEntriesToDeleteCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void GetProfileEntriesToDeleteCallback(
      std::optional<base::Value::Dict> profile_entries) {
    if (!profile_entries) {
      InvokeErrorCallback(service_path_, std::move(error_callback_),
                          "GetLoadableProfileEntriesFailed");
      // ProfileEntryDeleterCompleted will delete this.
      owner_->ProfileEntryDeleterCompleted(service_path_, guid_,
                                           false /* failed */);
      return;
    }

    for (const auto iter : *profile_entries) {
      std::string profile_path = StripQuotations(iter.first);
      std::string entry_path;
      if (iter.second.is_string()) {
        entry_path = iter.second.GetString();
      }
      if (profile_path.empty() || entry_path.empty()) {
        NET_LOG(ERROR) << "Failed to parse Profile Entry: " << profile_path
                       << ": " << entry_path;
        continue;
      }
      if (profile_delete_entries_.count(profile_path) != 0) {
        NET_LOG(ERROR) << "Multiple Profile Entries: " << profile_path << ": "
                       << entry_path;
        continue;
      }

      if (!restrict_to_profile_path_.empty() &&
          profile_path != restrict_to_profile_path_) {
        NET_LOG(DEBUG) << "Skip deleting Profile Entry: " << profile_path
                       << ": " << entry_path << " - removal is restricted to "
                       << restrict_to_profile_path_ << " profile";
        continue;
      }

      if (remove_confirmer_.has_value() &&
          !remove_confirmer_->Run(guid_, profile_path)) {
        continue;
      }

      NET_LOG(DEBUG) << "Delete Profile Entry: " << profile_path << ": "
                     << entry_path;
      profile_delete_entries_[profile_path] = entry_path;

      // If the ShillErrorCallback is executed synchronously, this object can
      // be deleted while this loop is still running.  While this shouldn't be
      // possible in practice, it should still be fixed to avoid problems in
      // the future.  Tracked in crbug.com/1019396.
      ShillProfileClient::Get()->DeleteEntry(
          dbus::ObjectPath(profile_path), entry_path,
          base::BindOnce(&ProfileEntryDeleter::ProfileEntryDeletedCallback,
                         weak_ptr_factory_.GetWeakPtr(), profile_path,
                         entry_path),
          base::BindOnce(&ProfileEntryDeleter::ShillErrorCallback,
                         weak_ptr_factory_.GetWeakPtr(), profile_path,
                         entry_path));
    }

    RunCallbackIfDone();
  }

  void ProfileEntryDeletedCallback(const std::string& profile_path,
                                   const std::string& entry) {
    NET_LOG(DEBUG) << "Profile Entry Deleted: " << profile_path << ": "
                   << entry;
    profile_delete_entries_.erase(profile_path);

    RunCallbackIfDone();
  }

  void RunCallbackIfDone() {
    if (!profile_delete_entries_.empty()) {
      return;
    }
    // Run the callback if this is the last pending deletion.
    if (!callback_.is_null()) {
      std::move(callback_).Run();
    }
    // ProfileEntryDeleterCompleted will delete this.
    owner_->ProfileEntryDeleterCompleted(service_path_, guid_,
                                         true /* success */);
  }

  void ShillErrorCallback(const std::string& profile_path,
                          const std::string& entry,
                          const std::string& dbus_error_name,
                          const std::string& dbus_error_message) {
    // Any Shill Error triggers a failure / error.
    network_handler::ShillErrorCallbackFunction(
        "GetLoadableProfileEntries Failed", profile_path,
        std::move(error_callback_), dbus_error_name, dbus_error_message);
    // Delete this even if there are pending deletions; any callbacks will
    // safely become no-ops (by invalidating the WeakPtrs).
    owner_->ProfileEntryDeleterCompleted(service_path_, guid_,
                                         false /* failed */);
  }

  raw_ptr<NetworkConfigurationHandler> owner_;  // Unowned
  std::string service_path_;
  // Non empty if the service has to be removed only from a single profile. This
  // value is the profile path of the profile in question.
  std::string restrict_to_profile_path_;
  std::string guid_;
  std::optional<RemoveConfirmer> remove_confirmer_;
  base::OnceClosure callback_;
  network_handler::ErrorCallback error_callback_;

  // Map of pending profile entry deletions, indexed by profile path.
  std::map<std::string, std::string> profile_delete_entries_;

  base::WeakPtrFactory<ProfileEntryDeleter> weak_ptr_factory_{this};
};

// NetworkConfigurationHandler

void NetworkConfigurationHandler::AddObserver(
    NetworkConfigurationObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkConfigurationHandler::RemoveObserver(
    NetworkConfigurationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkConfigurationHandler::GetShillProperties(
    const std::string& service_path,
    network_handler::ResultCallback callback) {
  NET_LOG(DEBUG) << "GetShillProperties: " << NetworkPathId(service_path);

  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (network_state &&
      (NetworkTypePattern::Tether().MatchesType(network_state->type()) ||
       network_state->IsNonShillCellularNetwork())) {
    // This is a Tether network or a Cellular network with no Service.
    // Provide properties from NetworkState.
    base::Value::Dict dictionary;
    network_state->GetStateProperties(&dictionary);
    std::move(callback).Run(service_path, std::move(dictionary));
    return;
  }
  ShillServiceClient::Get()->GetProperties(
      dbus::ObjectPath(service_path),
      base::BindOnce(&NetworkConfigurationHandler::GetPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     service_path));
}

void NetworkConfigurationHandler::SetShillProperties(
    const std::string& service_path,
    const base::Value::Dict& shill_properties,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  if (shill_properties.empty()) {
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
    return;
  }
  NET_LOG(USER) << "SetShillProperties: " << NetworkPathId(service_path);

  base::Value::Dict properties_to_set = shill_properties.Clone();

  // Make sure that the GUID is saved to Shill when setting properties.
  std::string guid = GetString(properties_to_set, shill::kGuidProperty);
  if (guid.empty()) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkState(service_path);
    guid = network_state ? network_state->guid()
                         : base::Uuid::GenerateRandomV4().AsLowercaseString();
    properties_to_set.Set(shill::kGuidProperty, guid);
  }

  LogConfigProperties("SetProperty", service_path, properties_to_set);

  // Clear error state when setting Shill properties.
  network_state_handler_->ClearLastErrorForNetwork(service_path);

  base::Value::Dict properties_copy = properties_to_set.Clone();
  ShillServiceClient::Get()->SetProperties(
      dbus::ObjectPath(service_path), properties_to_set,
      base::BindOnce(&NetworkConfigurationHandler::SetPropertiesSuccessCallback,
                     weak_ptr_factory_.GetWeakPtr(), service_path,
                     std::move(properties_copy), std::move(callback)),
      base::BindOnce(&NetworkConfigurationHandler::SetPropertiesErrorCallback,
                     weak_ptr_factory_.GetWeakPtr(), service_path,
                     std::move(error_callback)));
}

void NetworkConfigurationHandler::ClearShillProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  if (names.empty()) {
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
    return;
  }
  NET_LOG(USER) << "ClearShillProperties: " << NetworkPathId(service_path);
  for (std::vector<std::string>::const_iterator iter = names.begin();
       iter != names.end(); ++iter) {
    NET_LOG(DEBUG) << "ClearProperty: " << NetworkPathId(service_path) << ": "
                   << *iter;
  }
  ShillServiceClient::Get()->ClearProperties(
      dbus::ObjectPath(service_path), names,
      base::BindOnce(
          &NetworkConfigurationHandler::ClearPropertiesSuccessCallback,
          weak_ptr_factory_.GetWeakPtr(), service_path, names,
          std::move(callback)),
      base::BindOnce(&NetworkConfigurationHandler::ClearPropertiesErrorCallback,
                     weak_ptr_factory_.GetWeakPtr(), service_path,
                     std::move(error_callback)));
}

void NetworkConfigurationHandler::CreateShillConfiguration(
    const base::Value::Dict& shill_properties,
    network_handler::ServiceResultCallback callback,
    network_handler::ErrorCallback error_callback) {
  ShillManagerClient* manager = ShillManagerClient::Get();
  std::string type = GetString(shill_properties, shill::kTypeProperty);
  DCHECK(!type.empty());

  base::Value::Dict properties_to_set = shill_properties.Clone();

  NET_LOG(USER) << "CreateShillConfiguration: " << type << ": "
                << shill_property_util::GetNetworkIdFromProperties(
                       shill_properties);

  std::string profile_path =
      GetString(properties_to_set, shill::kProfileProperty);
  DCHECK(!profile_path.empty());

  // Make sure that the GUID is saved to Shill when configuring networks.
  std::string guid = GetString(properties_to_set, shill::kGuidProperty);
  if (guid.empty()) {
    guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    properties_to_set.Set(shill::kGuidProperty, guid);
  }

  LogConfigProperties("Configure", type, properties_to_set);
  base::Value::Dict properties_copy = properties_to_set.Clone();
  manager->ConfigureServiceForProfile(
      dbus::ObjectPath(profile_path), properties_to_set,
      base::BindOnce(&NetworkConfigurationHandler::ConfigurationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), profile_path, guid,
                     std::move(properties_copy), std::move(callback)),
      base::BindOnce(&NetworkConfigurationHandler::ConfigurationFailed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(error_callback)));
}

void NetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    std::optional<RemoveConfirmer> remove_confirmer,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  RemoveConfigurationFromProfile(service_path, "", std::move(remove_confirmer),
                                 std::move(callback),
                                 std::move(error_callback));
}

void NetworkConfigurationHandler::RemoveConfigurationFromCurrentProfile(
    const std::string& service_path,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);

  if (!network_state || network_state->profile_path().empty()) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        "NetworkNotConfigured");
    return;
  }
  RemoveConfigurationFromProfile(service_path, network_state->profile_path(),
                                 /*remove_confirmer=*/std::nullopt,
                                 std::move(callback),
                                 std::move(error_callback));
}

void NetworkConfigurationHandler::RemoveConfigurationFromProfile(
    const std::string& service_path,
    const std::string& profile_path,
    std::optional<RemoveConfirmer> remove_confirmer,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  // Service.Remove is not reliable. Instead, request the profile entries
  // for the service and remove each entry.
  if (base::Contains(profile_entry_deleters_, service_path)) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        "RemoveConfigurationInProgress");
    return;
  }

  std::string guid;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (network_state) {
    guid = network_state->guid();
  }
  NET_LOG(USER) << "Remove Configuration: " << NetworkPathId(service_path)
                << " from profiles: "
                << (!profile_path.empty() ? profile_path : "all");
  for (auto& observer : observers_) {
    observer.OnBeforeConfigurationRemoved(service_path, guid);
  }
  ProfileEntryDeleter* deleter = new ProfileEntryDeleter(
      this, service_path, guid, std::move(remove_confirmer),
      std::move(callback), std::move(error_callback));
  if (!profile_path.empty()) {
    deleter->RestrictToProfilePath(profile_path);
  }
  profile_entry_deleters_[service_path] = base::WrapUnique(deleter);
  deleter->Run();
}

void NetworkConfigurationHandler::SetNetworkProfile(
    const std::string& service_path,
    const std::string& profile_path,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "SetNetworkProfile: " << NetworkPathId(service_path) << ": "
                << profile_path;
  base::Value profile_path_value(profile_path);
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(service_path), shill::kProfileProperty,
      profile_path_value,
      base::BindOnce(&NetworkConfigurationHandler::SetNetworkProfileCompleted,
                     weak_ptr_factory_.GetWeakPtr(), service_path, profile_path,
                     std::move(callback)),
      base::BindOnce(&SetNetworkProfileErrorCallback, service_path,
                     profile_path, std::move(error_callback)));
}

void NetworkConfigurationHandler::SetManagerProperty(
    const std::string& property_name,
    const base::Value& value) {
  NET_LOG(USER) << "SetManagerProperty: " << property_name << ": " << value;
  ShillManagerClient::Get()->SetProperty(
      property_name, value, base::DoNothing(),
      base::BindOnce(&ManagerSetPropertiesErrorCallback));
}

// NetworkStateHandlerObserver methods
void NetworkConfigurationHandler::NetworkListChanged() {
  for (auto iter = configure_callbacks_.begin();
       iter != configure_callbacks_.end();) {
    const std::string& service_path = iter->first;
    const NetworkState* state =
        network_state_handler_->GetNetworkStateFromServicePath(service_path,
                                                               true);
    if (!state) {
      NET_LOG(ERROR) << "Configured network not in list: " << service_path;
      ++iter;
      continue;
    }
    network_handler::ServiceResultCallback& callback = iter->second;
    std::move(callback).Run(service_path, state->guid());
    iter = configure_callbacks_.erase(iter);
  }
}

void NetworkConfigurationHandler::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
  for (auto& observer : observers_) {
    observer.OnShuttingDown();
  }
}

// NetworkConfigurationHandler Private methods

NetworkConfigurationHandler::NetworkConfigurationHandler()
    : network_state_handler_(nullptr) {}

NetworkConfigurationHandler::~NetworkConfigurationHandler() {
  // Make sure that this has been removed as a NetworkStateHandler observer.
  if (network_state_handler_) {
    OnShuttingDown();
  }
}

void NetworkConfigurationHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler) {
  network_state_handler_ = network_state_handler;
  network_device_handler_ = network_device_handler;

  // Observer is removed in OnShuttingDown() observer override.
  network_state_handler_observer_.Observe(network_state_handler_.get());
}

void NetworkConfigurationHandler::ConfigurationFailed(
    network_handler::ErrorCallback error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  std::string error_name = GetErrorName(dbus_error_name, dbus_error_message,
                                        "Config.CreateConfiguration Failed");
  network_handler::ShillErrorCallbackFunction(
      error_name, "", std::move(error_callback), dbus_error_name,
      dbus_error_message);
}

void NetworkConfigurationHandler::ConfigurationCompleted(
    const std::string& profile_path,
    const std::string& guid,
    base::Value::Dict configure_properties,
    network_handler::ServiceResultCallback callback,
    const dbus::ObjectPath& service_path) {
  // It is possible that the newly-configured network was already being tracked
  // by |network_state_handler_|. If this is the case, clear any existing error
  // from a previous connection attempt.
  network_state_handler_->ClearLastErrorForNetwork(service_path.value());

  // |configure_callbacks_| will get triggered when NetworkStateHandler notifies
  // this that a state list update has occurred, which will be triggered by the
  // following RequestUpdateForNetwork call. |service_path| is unique per
  // configuration.
  configure_callbacks_.insert(std::make_pair(
      service_path.value(),
      base::BindOnce(&NetworkConfigurationHandler::NotifyConfigurationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));

  // Shill should send a network list update, but to ensure that Shill sends
  // the newly configured properties immediately, request an update here.
  network_state_handler_->RequestUpdateForNetwork(service_path.value());
}

void NetworkConfigurationHandler::NotifyConfigurationCompleted(
    network_handler::ServiceResultCallback callback,
    const std::string& service_path,
    const std::string& guid) {
  for (auto& observer : observers_) {
    observer.OnConfigurationCreated(service_path, guid);
  }

  if (callback.is_null()) {
    return;
  }

  std::move(callback).Run(service_path, guid);
}

void NetworkConfigurationHandler::ProfileEntryDeleterCompleted(
    const std::string& service_path,
    const std::string& guid,
    bool success) {
  if (success) {
    // Since the configuration was removed, clear any associated error to ensure
    // that the UI does not display stale errors from a previous configuration.
    network_state_handler_->ClearLastErrorForNetwork(service_path);

    for (auto& observer : observers_) {
      observer.OnConfigurationRemoved(service_path, guid);
    }
  }
  auto iter = profile_entry_deleters_.find(service_path);
  DCHECK(iter != profile_entry_deleters_.end());
  profile_entry_deleters_.erase(iter);
}

void NetworkConfigurationHandler::SetNetworkProfileCompleted(
    const std::string& service_path,
    const std::string& profile_path,
    base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void NetworkConfigurationHandler::GetPropertiesCallback(
    network_handler::ResultCallback callback,
    const std::string& service_path,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    // Because network services are added and removed frequently, we will see
    // failures regularly, so don't log these.
    std::move(callback).Run(service_path, std::nullopt);
    return;
  }

  // Get the correct name from WifiHex if necessary.
  std::string name =
      shill_property_util::GetNameFromProperties(service_path, *properties);
  if (!name.empty()) {
    properties->Set(shill::kNameProperty, name);
  }

  // Get the GUID property from NetworkState if it is not set in Shill.
  const std::string* guid =
      properties->FindString(::onc::network_config::kGUID);
  if (!guid || guid->empty()) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkState(service_path);
    if (network_state) {
      properties->Set(::onc::network_config::kGUID, network_state->guid());
    }
  }

  std::move(callback).Run(service_path, std::move(*properties));
}

void NetworkConfigurationHandler::SetPropertiesSuccessCallback(
    const std::string& service_path,
    base::Value::Dict set_properties,
    base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state) {
    return;  // Network no longer exists, do not notify or request update.
  }

  for (auto& observer : observers_) {
    observer.OnConfigurationModified(service_path, network_state->guid(),
                                     &set_properties);
  }

  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::SetPropertiesErrorCallback(
    const std::string& service_path,
    network_handler::ErrorCallback error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  std::string error_name = GetErrorName(dbus_error_name, dbus_error_message,
                                        "Config.SetProperties Failed");
  network_handler::ShillErrorCallbackFunction(
      error_name, service_path, std::move(error_callback), dbus_error_name,
      dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearPropertiesSuccessCallback(
    const std::string& service_path,
    const std::vector<std::string>& names,
    base::OnceClosure callback,
    const base::Value::List& result) {
  const std::string kClearPropertiesFailedError("Error.ClearPropertiesFailed");
  DCHECK(names.size() == result.size())
      << "Incorrect result size from ClearProperties.";

  for (size_t i = 0; i < result.size(); ++i) {
    bool success = false;
    if (result[i].is_bool()) {
      success = result[i].GetBool();
    }
    if (!success) {
      // If a property was cleared that has never been set, the clear will fail.
      // We do not track which properties have been set, so just log the error.
      NET_LOG(ERROR) << "ClearProperties Failed: "
                     << NetworkPathId(service_path) << ": " << names[i];
    }
  }

  if (!callback.is_null()) {
    std::move(callback).Run();
  }
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearPropertiesErrorCallback(
    const std::string& service_path,
    network_handler::ErrorCallback error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.ClearProperties Failed", service_path, std::move(error_callback),
      dbus_error_name, dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

// static
std::unique_ptr<NetworkConfigurationHandler>
NetworkConfigurationHandler::InitializeForTest(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler) {
  auto handler = base::WrapUnique(new NetworkConfigurationHandler());
  handler->Init(network_state_handler, network_device_handler);
  return handler;
}

}  // namespace ash
