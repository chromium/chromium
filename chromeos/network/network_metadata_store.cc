// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_metadata_store.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char kNetworkMetadataPref[] = "network_metadata";
const char kLastConnectedTimestampPref[] = "last_connected_timestamp";
const char kIsFromSync[] = "is_from_sync";
const char kOwner[] = "owner";
const char kExternalModifications[] = "external_modifications";
const char kBadPassword[] = "bad_password";
const char kCustomApnList[] = "custom_apn_list";

std::string GetPath(const std::string& guid, const std::string& subkey) {
  return base::StringPrintf("%s.%s", guid.c_str(), subkey.c_str());
}

base::Value CreateOrCloneListValue(const base::Value* list) {
  if (list)
    return list->Clone();

  return base::ListValue();
}

bool ListContains(const base::Value* list, const std::string& value) {
  if (!list)
    return false;
  base::Value::ConstListView list_view = list->GetList();
  return std::find(list_view.begin(), list_view.end(), base::Value(value)) !=
         list_view.end();
}

}  // namespace

// static
void NetworkMetadataStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kNetworkMetadataPref);
}

NetworkMetadataStore::NetworkMetadataStore(
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    PrefService* profile_pref_service,
    PrefService* device_pref_service,
    bool is_enterprise_managed)
    : network_configuration_handler_(network_configuration_handler),
      network_connection_handler_(network_connection_handler),
      network_state_handler_(network_state_handler),
      profile_pref_service_(profile_pref_service),
      device_pref_service_(device_pref_service),
      is_enterprise_managed_(is_enterprise_managed) {
  if (network_connection_handler_) {
    network_connection_handler_->AddObserver(this);
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->AddObserver(this);
  }
  if (LoginState::IsInitialized()) {
    LoginState::Get()->AddObserver(this);
  }
}

NetworkMetadataStore::~NetworkMetadataStore() {
  if (network_connection_handler_) {
    network_connection_handler_->RemoveObserver(this);
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->RemoveObserver(this);
  }
  if (LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void NetworkMetadataStore::LoggedInStateChanged() {
  OwnSharedNetworksOnFirstUserLogin();
}

void NetworkMetadataStore::OwnSharedNetworksOnFirstUserLogin() {
  if (is_enterprise_managed_ || !network_state_handler_ ||
      !user_manager::UserManager::IsInitialized()) {
    return;
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  if (!user_manager->IsCurrentUserNew() ||
      !user_manager->IsCurrentUserOwner()) {
    return;
  }

  NET_LOG(EVENT) << "Taking ownership of shared networks.";
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::WiFi(), true,
                                               false, 0, &networks);
  for (const chromeos::NetworkState* network : networks) {
    if (network->IsPrivate()) {
      continue;
    }

    SetIsCreatedByUser(network->guid());
  }
}

void NetworkMetadataStore::ConnectSucceeded(const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network || network->type() != shill::kTypeWifi) {
    return;
  }

  bool is_first_connection =
      GetLastConnectedTimestamp(network->guid()).is_zero();

  SetLastConnectedTimestamp(network->guid(),
                            base::Time::Now().ToDeltaSinceWindowsEpoch());
  SetPref(network->guid(), kBadPassword, base::Value(false));

  if (is_first_connection) {
    for (auto& observer : observers_) {
      observer.OnFirstConnectionToNetwork(network->guid());
    }
  }
}

void NetworkMetadataStore::ConnectFailed(const std::string& service_path,
                                         const std::string& error_name) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  // Only set kBadPassword for Wi-Fi networks which have never had a successful
  // connection with the current password.  |error_name| is always set to
  // "connect-failed", network->GetError() contains the real cause.
  if (!network || network->type() != shill::kTypeWifi ||
      network->GetError() != shill::kErrorBadPassphrase ||
      !GetLastConnectedTimestamp(network->guid()).is_zero()) {
    return;
  }

  SetPref(network->guid(), kBadPassword, base::Value(true));
}

void NetworkMetadataStore::OnConfigurationCreated(
    const std::string& service_path,
    const std::string& guid) {
  SetIsCreatedByUser(guid);
}

void NetworkMetadataStore::SetIsCreatedByUser(const std::string& network_guid) {
  if (!user_manager::UserManager::IsInitialized())
    return;

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user) {
    NET_LOG(EVENT)
        << "Network added with no active user, owner metadata not recorded.";
    return;
  }

  SetPref(network_guid, kOwner, base::Value(user->username_hash()));

  for (auto& observer : observers_) {
    observer.OnNetworkCreated(network_guid);
  }
}

void NetworkMetadataStore::UpdateExternalModifications(
    const std::string& network_guid,
    const std::string& field) {
  const base::Value* fields = GetPref(network_guid, kExternalModifications);
  if (GetIsCreatedByUser(network_guid)) {
    if (ListContains(fields, field)) {
      base::Value writeable_fields = CreateOrCloneListValue(fields);
      writeable_fields.EraseListValue(base::Value(field));
      SetPref(network_guid, kExternalModifications,
              std::move(writeable_fields));
    }
  } else if (!ListContains(fields, field)) {
    base::Value writeable_fields = CreateOrCloneListValue(fields);
    writeable_fields.Append(base::Value(field));
    SetPref(network_guid, kExternalModifications, std::move(writeable_fields));
  }
}

void NetworkMetadataStore::OnConfigurationModified(
    const std::string& service_path,
    const std::string& guid,
    base::DictionaryValue* set_properties) {
  if (!set_properties) {
    return;
  }

  SetPref(guid, kIsFromSync, base::Value(false));

  if (set_properties->HasKey(shill::kProxyConfigProperty)) {
    UpdateExternalModifications(guid, shill::kProxyConfigProperty);
  }
  if (set_properties->FindPath(
          base::StringPrintf("%s.%s", shill::kStaticIPConfigProperty,
                             shill::kNameServersProperty))) {
    UpdateExternalModifications(guid, shill::kNameServersProperty);
  }

  // Only clear last connected if the passphrase changes.  Other settings
  // (autoconnect, dns, etc.) won't affect the ability to connect to a network.
  if (set_properties->HasKey(shill::kPassphraseProperty)) {
    SetPref(guid, kLastConnectedTimestampPref, base::Value(0));
  }

  for (auto& observer : observers_) {
    observer.OnNetworkUpdate(guid, set_properties);
  }
}

void NetworkMetadataStore::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& network_guid) {
  RemoveNetworkFromPref(network_guid, device_pref_service_);
  RemoveNetworkFromPref(network_guid, profile_pref_service_);
}

void NetworkMetadataStore::RemoveNetworkFromPref(
    const std::string& network_guid,
    PrefService* pref_service) {
  if (!pref_service) {
    return;
  }

  const base::DictionaryValue* dict =
      pref_service->GetDictionary(kNetworkMetadataPref);
  if (!dict || !dict->HasKey(network_guid)) {
    return;
  }

  base::Value writeable_dict = dict->Clone();
  if (!writeable_dict.RemoveKey(network_guid)) {
    return;
  }

  pref_service->Set(kNetworkMetadataPref, writeable_dict);
}

void NetworkMetadataStore::SetIsConfiguredBySync(
    const std::string& network_guid) {
  SetPref(network_guid, kIsFromSync, base::Value(true));
}

base::TimeDelta NetworkMetadataStore::GetLastConnectedTimestamp(
    const std::string& network_guid) {
  const base::Value* timestamp =
      GetPref(network_guid, kLastConnectedTimestampPref);

  if (!timestamp || !timestamp->is_double()) {
    return base::TimeDelta();
  }

  return base::TimeDelta::FromMillisecondsD(timestamp->GetDouble());
}

void NetworkMetadataStore::SetLastConnectedTimestamp(
    const std::string& network_guid,
    const base::TimeDelta& timestamp) {
  double timestamp_f = timestamp.InMillisecondsF();
  SetPref(network_guid, kLastConnectedTimestampPref, base::Value(timestamp_f));
}

bool NetworkMetadataStore::GetIsConfiguredBySync(
    const std::string& network_guid) {
  const base::Value* is_from_sync = GetPref(network_guid, kIsFromSync);
  if (!is_from_sync) {
    return false;
  }

  return is_from_sync->GetBool();
}

bool NetworkMetadataStore::GetIsCreatedByUser(const std::string& network_guid) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);
  if (network && network->IsPrivate())
    return true;

  const base::Value* owner = GetPref(network_guid, kOwner);
  if (!owner) {
    return false;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user) {
    return false;
  }

  return owner->GetString() == user->username_hash();
}

bool NetworkMetadataStore::GetIsFieldExternallyModified(
    const std::string& network_guid,
    const std::string& field) {
  const base::Value* fields = GetPref(network_guid, kExternalModifications);
  return ListContains(fields, field);
}

bool NetworkMetadataStore::GetHasBadPassword(const std::string& network_guid) {
  const base::Value* has_bad_password = GetPref(network_guid, kBadPassword);

  // If the pref is not set, default to false.
  if (!has_bad_password) {
    return false;
  }

  return has_bad_password->GetBool();
}

void NetworkMetadataStore::SetCustomAPNList(const std::string& network_guid,
                                            base::Value list) {
  SetPref(network_guid, kCustomApnList, std::move(list));
}

const base::Value* NetworkMetadataStore::GetCustomAPNList(
    const std::string& network_guid) {
  return GetPref(network_guid, kCustomApnList);
}

void NetworkMetadataStore::SetPref(const std::string& network_guid,
                                   const std::string& key,
                                   base::Value value) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);

  if (network && network->IsPrivate() && profile_pref_service_) {
    base::Value profile_dict =
        profile_pref_service_->GetDictionary(kNetworkMetadataPref)->Clone();
    profile_dict.SetPath(GetPath(network_guid, key), std::move(value));
    profile_pref_service_->Set(kNetworkMetadataPref, profile_dict);
    return;
  }

  base::Value device_dict =
      device_pref_service_->GetDictionary(kNetworkMetadataPref)->Clone();
  device_dict.SetPath(GetPath(network_guid, key), std::move(value));
  device_pref_service_->Set(kNetworkMetadataPref, device_dict);
}

const base::Value* NetworkMetadataStore::GetPref(
    const std::string& network_guid,
    const std::string& key) {
  if (!network_state_handler_) {
    return nullptr;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);

  if (network && network->IsPrivate() && profile_pref_service_) {
    const base::Value* profile_dict =
        profile_pref_service_->GetDictionary(kNetworkMetadataPref);
    const base::Value* value =
        profile_dict->FindPath(GetPath(network_guid, key));
    if (value)
      return value;
  }

  const base::Value* device_dict =
      device_pref_service_->GetDictionary(kNetworkMetadataPref);
  return device_dict->FindPath(GetPath(network_guid, key));
}

void NetworkMetadataStore::AddObserver(NetworkMetadataObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkMetadataStore::RemoveObserver(NetworkMetadataObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
