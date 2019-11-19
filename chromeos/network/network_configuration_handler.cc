// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_configuration_handler.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Strip surrounding "" from keys (if present).
std::string StripQuotations(const std::string& in_str) {
  size_t len = in_str.length();
  if (len >= 2 && in_str[0] == '"' && in_str[len - 1] == '"')
    return in_str.substr(1, len - 2);
  return in_str;
}

void InvokeErrorCallback(const std::string& service_path,
                         const network_handler::ErrorCallback& error_callback,
                         const std::string& error_name) {
  std::string error_msg = "Config Error: " + error_name;
  NET_LOG(ERROR) << error_msg << ": " << service_path;
  network_handler::RunErrorCallback(error_callback, service_path, error_name,
                                    error_msg);
}

void SetNetworkProfileErrorCallback(
    const std::string& service_path,
    const std::string& profile_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.SetNetworkProfile Failed: " + profile_path, service_path,
      error_callback, dbus_error_name, dbus_error_message);
}

void ManagerSetPropertiesErrorCallback(
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "ShillManagerClient.SetProperties Failed", std::string(), error_callback,
      dbus_error_name, dbus_error_message);
}

void LogConfigProperties(const std::string& desc,
                         const std::string& path,
                         const base::DictionaryValue& properties) {
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
    std::string v = "******";
    if (shill_property_util::IsLoggableShillProperty(iter.key()))
      base::JSONWriter::Write(iter.value(), &v);
    NET_LOG(USER) << desc << ": " << path + "." + iter.key() + "=" + v;
  }
}

// Returns recognized dbus error names or |default_error_name|.
// TODO(stevenjb): Expand this list and update
// network_element::AddErrorLocalizedStrings.
std::string GetErrorName(const std::string& dbus_error_name,
                         const std::string& default_error_name) {
  if (dbus_error_name == shill::kErrorResultInvalidPassphrase)
    return dbus_error_name;
  return default_error_name;
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
                      const base::Closure& callback,
                      const network_handler::ErrorCallback& error_callback)
      : owner_(handler),
        service_path_(service_path),
        guid_(guid),
        callback_(callback),
        error_callback_(error_callback) {}

  void RestrictToProfilePath(const std::string& profile_path) {
    restrict_to_profile_path_ = profile_path;
  }

  void Run() {
    ShillServiceClient::Get()->GetLoadableProfileEntries(
        dbus::ObjectPath(service_path_),
        base::Bind(&ProfileEntryDeleter::GetProfileEntriesToDeleteCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void GetProfileEntriesToDeleteCallback(
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& profile_entries) {
    if (call_status != DBUS_METHOD_CALL_SUCCESS) {
      InvokeErrorCallback(service_path_, error_callback_,
                          "GetLoadableProfileEntriesFailed");
      // ProfileEntryDeleterCompleted will delete this.
      owner_->ProfileEntryDeleterCompleted(service_path_, guid_,
                                           false /* failed */);
      return;
    }

    for (base::DictionaryValue::Iterator iter(profile_entries); !iter.IsAtEnd();
         iter.Advance()) {
      std::string profile_path = StripQuotations(iter.key());
      std::string entry_path;
      iter.value().GetAsString(&entry_path);
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

      NET_LOG(DEBUG) << "Delete Profile Entry: " << profile_path << ": "
                     << entry_path;
      profile_delete_entries_[profile_path] = entry_path;

      // If the ShillErrorCallback is executed synchronously, this object can
      // be deleted while this loop is still running.  While this shouldn't be
      // possible in practice, it should still be fixed to avoid problems in
      // the future.  Tracked in crbug.com/1019396.
      ShillProfileClient::Get()->DeleteEntry(
          dbus::ObjectPath(profile_path), entry_path,
          base::Bind(&ProfileEntryDeleter::ProfileEntryDeletedCallback,
                     weak_ptr_factory_.GetWeakPtr(), profile_path, entry_path),
          base::Bind(&ProfileEntryDeleter::ShillErrorCallback,
                     weak_ptr_factory_.GetWeakPtr(), profile_path, entry_path));
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
    if (!profile_delete_entries_.empty())
      return;
    // Run the callback if this is the last pending deletion.
    if (!callback_.is_null())
      callback_.Run();
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
        "GetLoadableProfileEntries Failed", profile_path, error_callback_,
        dbus_error_name, dbus_error_message);
    // Delete this even if there are pending deletions; any callbacks will
    // safely become no-ops (by invalidating the WeakPtrs).
    owner_->ProfileEntryDeleterCompleted(service_path_, guid_,
                                         false /* failed */);
  }

  NetworkConfigurationHandler* owner_;  // Unowned
  std::string service_path_;
  // Non empty if the service has to be removed only from a single profile. This
  // value is the profile path of the profile in question.
  std::string restrict_to_profile_path_;
  std::string guid_;
  base::Closure callback_;
  network_handler::ErrorCallback error_callback_;

  // Map of pending profile entry deletions, indexed by profile path.
  std::map<std::string, std::string> profile_delete_entries_;

  base::WeakPtrFactory<ProfileEntryDeleter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileEntryDeleter);
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
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG(USER) << "GetShillProperties: " << service_path;

  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (network_state &&
      (NetworkTypePattern::Tether().MatchesType(network_state->type()) ||
       network_state->IsDefaultCellular())) {
    // This is a Tether network or a Cellular network with no Service.
    // Provide properties from NetworkState.
    base::DictionaryValue dictionary;
    network_state->GetStateProperties(&dictionary);
    callback.Run(service_path, dictionary);
    return;
  }
  ShillServiceClient::Get()->GetProperties(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConfigurationHandler::GetPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback,
                 service_path));
}

void NetworkConfigurationHandler::SetShillProperties(
    const std::string& service_path,
    const base::DictionaryValue& shill_properties,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (shill_properties.empty()) {
    if (!callback.is_null())
      callback.Run();
    return;
  }
  NET_LOG(USER) << "SetShillProperties: " << service_path;

  std::unique_ptr<base::DictionaryValue> properties_to_set(
      shill_properties.DeepCopy());

  // Make sure that the GUID is saved to Shill when setting properties.
  std::string guid;
  properties_to_set->GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  if (guid.empty()) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkState(service_path);
    guid = network_state ? network_state->guid() : base::GenerateGUID();
    properties_to_set->SetKey(shill::kGuidProperty, base::Value(guid));
  }

  LogConfigProperties("SetProperty", service_path, *properties_to_set);

  // Clear error state when setting Shill properties.
  network_state_handler_->ClearLastErrorForNetwork(service_path);

  std::unique_ptr<base::DictionaryValue> properties_copy(
      properties_to_set->DeepCopy());
  ShillServiceClient::Get()->SetProperties(
      dbus::ObjectPath(service_path), *properties_to_set,
      base::Bind(&NetworkConfigurationHandler::SetPropertiesSuccessCallback,
                 weak_ptr_factory_.GetWeakPtr(), service_path,
                 base::Passed(&properties_copy), callback),
      base::Bind(&NetworkConfigurationHandler::SetPropertiesErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), service_path, error_callback));
}

void NetworkConfigurationHandler::ClearShillProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  if (names.empty()) {
    if (!callback.is_null())
      callback.Run();
    return;
  }
  NET_LOG(USER) << "ClearShillProperties: " << service_path;
  for (std::vector<std::string>::const_iterator iter = names.begin();
       iter != names.end(); ++iter) {
    NET_LOG(DEBUG) << "ClearProperty: " << service_path << "." << *iter;
  }
  ShillServiceClient::Get()->ClearProperties(
      dbus::ObjectPath(service_path), names,
      base::Bind(&NetworkConfigurationHandler::ClearPropertiesSuccessCallback,
                 weak_ptr_factory_.GetWeakPtr(), service_path, names, callback),
      base::Bind(&NetworkConfigurationHandler::ClearPropertiesErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), service_path, error_callback));
}

void NetworkConfigurationHandler::CreateShillConfiguration(
    const base::DictionaryValue& shill_properties,
    const network_handler::ServiceResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  ShillManagerClient* manager = ShillManagerClient::Get();
  std::string type;
  shill_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  DCHECK(!type.empty());

  std::string network_id =
      shill_property_util::GetNetworkIdFromProperties(shill_properties);

  std::unique_ptr<base::DictionaryValue> properties_to_set(
      shill_properties.DeepCopy());

  NET_LOG(USER) << "CreateShillConfiguration: " << type << ": " << network_id;

  std::string profile_path;
  properties_to_set->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                   &profile_path);
  DCHECK(!profile_path.empty());

  // Make sure that the GUID is saved to Shill when configuring networks.
  std::string guid;
  properties_to_set->GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  if (guid.empty()) {
    guid = base::GenerateGUID();
    properties_to_set->SetKey(shill::kGuidProperty, base::Value(guid));
  }

  LogConfigProperties("Configure", type, *properties_to_set);

  std::unique_ptr<base::DictionaryValue> properties_copy(
      properties_to_set->DeepCopy());
  manager->ConfigureServiceForProfile(
      dbus::ObjectPath(profile_path), *properties_to_set,
      base::Bind(&NetworkConfigurationHandler::ConfigurationCompleted,
                 weak_ptr_factory_.GetWeakPtr(), profile_path,
                 base::Passed(&properties_copy), callback),
      base::Bind(&NetworkConfigurationHandler::ConfigurationFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void NetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  RemoveConfigurationFromProfile(service_path, "", callback, error_callback);
}

void NetworkConfigurationHandler::RemoveConfigurationFromCurrentProfile(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);

  if (!network_state || network_state->profile_path().empty()) {
    InvokeErrorCallback(service_path, error_callback, "NetworkNotConfigured");
    return;
  }
  RemoveConfigurationFromProfile(service_path, network_state->profile_path(),
                                 callback, error_callback);
}

void NetworkConfigurationHandler::RemoveConfigurationFromProfile(
    const std::string& service_path,
    const std::string& profile_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  // Service.Remove is not reliable. Instead, request the profile entries
  // for the service and remove each entry.
  if (base::Contains(profile_entry_deleters_, service_path)) {
    InvokeErrorCallback(service_path, error_callback,
                        "RemoveConfigurationInProgress");
    return;
  }

  std::string guid;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (network_state)
    guid = network_state->guid();
  NET_LOG(USER) << "Remove Configuration: " << service_path
                << " from profiles: "
                << (!profile_path.empty() ? profile_path : "all");
  ProfileEntryDeleter* deleter = new ProfileEntryDeleter(
      this, service_path, guid, callback, error_callback);
  if (!profile_path.empty())
    deleter->RestrictToProfilePath(profile_path);
  profile_entry_deleters_[service_path] = base::WrapUnique(deleter);
  deleter->Run();
}

void NetworkConfigurationHandler::SetNetworkProfile(
    const std::string& service_path,
    const std::string& profile_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG(USER) << "SetNetworkProfile: " << service_path << ": "
                << profile_path;
  base::Value profile_path_value(profile_path);
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(service_path), shill::kProfileProperty,
      profile_path_value,
      base::Bind(&NetworkConfigurationHandler::SetNetworkProfileCompleted,
                 weak_ptr_factory_.GetWeakPtr(), service_path, profile_path,
                 callback),
      base::Bind(&SetNetworkProfileErrorCallback, service_path, profile_path,
                 error_callback));
}

void NetworkConfigurationHandler::SetManagerProperty(
    const std::string& property_name,
    const base::Value& value,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG(USER) << "SetManagerProperty: " << property_name << ": " << value;
  ShillManagerClient::Get()->SetProperty(
      property_name, value, callback,
      base::Bind(&ManagerSetPropertiesErrorCallback, error_callback));
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
    callback.Run(service_path, state->guid());
    iter = configure_callbacks_.erase(iter);
  }
}

void NetworkConfigurationHandler::OnShuttingDown() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

// NetworkConfigurationHandler Private methods

NetworkConfigurationHandler::NetworkConfigurationHandler()
    : network_state_handler_(nullptr) {}

NetworkConfigurationHandler::~NetworkConfigurationHandler() {
  // Make sure that this has been removed as a NetworkStateHandler observer.
  if (network_state_handler_)
    OnShuttingDown();
}

void NetworkConfigurationHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler) {
  network_state_handler_ = network_state_handler;
  network_device_handler_ = network_device_handler;

  // Observer is removed in OnShuttingDown() observer override.
  network_state_handler_->AddObserver(this, FROM_HERE);
}

void NetworkConfigurationHandler::ConfigurationFailed(
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  std::string error_name =
      GetErrorName(dbus_error_name, "Config.CreateConfiguration Failed");
  network_handler::ShillErrorCallbackFunction(
      error_name, "", error_callback, dbus_error_name, dbus_error_message);
}

void NetworkConfigurationHandler::ConfigurationCompleted(
    const std::string& profile_path,
    std::unique_ptr<base::DictionaryValue> configure_properties,
    const network_handler::ServiceResultCallback& callback,
    const dbus::ObjectPath& service_path) {
  // It is possible that the newly-configured network was already being tracked
  // by |network_state_handler_|. If this is the case, clear any existing error
  // from a previous connection attempt.
  network_state_handler_->ClearLastErrorForNetwork(service_path.value());

  // Shill should send a network list update, but to ensure that Shill sends
  // the newly configured properties immediately, request an update here.
  network_state_handler_->RequestUpdateForNetwork(service_path.value());

  if (callback.is_null())
    return;

  // |configure_callbacks_| will get triggered when NetworkStateHandler
  // notifies this that a state list update has occurred. |service_path|
  // is unique per configuration.
  configure_callbacks_.insert(std::make_pair(service_path.value(), callback));
}

void NetworkConfigurationHandler::ProfileEntryDeleterCompleted(
    const std::string& service_path,
    const std::string& guid,
    bool success) {
  if (success) {
    // Since the configuration was removed, clear any associated error to ensure
    // that the UI does not display stale errors from a previous configuration.
    network_state_handler_->ClearLastErrorForNetwork(service_path);

    for (auto& observer : observers_)
      observer.OnConfigurationRemoved(service_path, guid);
  }
  auto iter = profile_entry_deleters_.find(service_path);
  DCHECK(iter != profile_entry_deleters_.end());
  profile_entry_deleters_.erase(iter);
}

void NetworkConfigurationHandler::SetNetworkProfileCompleted(
    const std::string& service_path,
    const std::string& profile_path,
    const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
}

void NetworkConfigurationHandler::GetPropertiesCallback(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    // Because network services are added and removed frequently, we will see
    // failures regularly, so don't log these.
    network_handler::RunErrorCallback(error_callback, service_path,
                                      network_handler::kDBusFailedError,
                                      network_handler::kDBusFailedErrorMessage);
    return;
  }
  if (callback.is_null())
    return;

  // Get the correct name from WifiHex if necessary.
  std::unique_ptr<base::DictionaryValue> properties_copy(properties.DeepCopy());
  std::string name =
      shill_property_util::GetNameFromProperties(service_path, properties);
  if (!name.empty())
    properties_copy->SetKey(shill::kNameProperty, base::Value(name));

  // Get the GUID property from NetworkState if it is not set in Shill.
  std::string guid;
  properties.GetStringWithoutPathExpansion(::onc::network_config::kGUID, &guid);
  if (guid.empty()) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkState(service_path);
    if (network_state) {
      properties_copy->SetKey(::onc::network_config::kGUID,
                              base::Value(network_state->guid()));
    }
  }

  callback.Run(service_path, *properties_copy.get());
}

void NetworkConfigurationHandler::SetPropertiesSuccessCallback(
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> set_properties,
    const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state)
    return;  // Network no longer exists, do not notify or request update.

  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::SetPropertiesErrorCallback(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  std::string error_name =
      GetErrorName(dbus_error_name, "Config.SetProperties Failed");
  network_handler::ShillErrorCallbackFunction(error_name, service_path,
                                              error_callback, dbus_error_name,
                                              dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearPropertiesSuccessCallback(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const base::ListValue& result) {
  const std::string kClearPropertiesFailedError("Error.ClearPropertiesFailed");
  DCHECK(names.size() == result.GetSize())
      << "Incorrect result size from ClearProperties.";

  for (size_t i = 0; i < result.GetSize(); ++i) {
    bool success = false;
    result.GetBoolean(i, &success);
    if (!success) {
      // If a property was cleared that has never been set, the clear will fail.
      // We do not track which properties have been set, so just log the error.
      NET_LOG(ERROR) << "ClearProperties Failed: " << service_path << ": "
                     << names[i];
    }
  }

  if (!callback.is_null())
    callback.Run();
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

void NetworkConfigurationHandler::ClearPropertiesErrorCallback(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "Config.ClearProperties Failed", service_path, error_callback,
      dbus_error_name, dbus_error_message);
  // Some properties may have changed so request an update regardless.
  network_state_handler_->RequestUpdateForNetwork(service_path);
}

// static
NetworkConfigurationHandler* NetworkConfigurationHandler::InitializeForTest(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler) {
  NetworkConfigurationHandler* handler = new NetworkConfigurationHandler();
  handler->Init(network_state_handler, network_device_handler);
  return handler;
}

}  // namespace chromeos
