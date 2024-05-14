// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_metadata_store.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {
const char kNetworkMetadataPref[] = "network_metadata";
const char kLastConnectedTimestampPref[] = "last_connected_timestamp";
const char kCreationTimestamp[] = "creation_timestamp";
const char kIsFromSync[] = "is_from_sync";
const char kOwner[] = "owner";
const char kExternalModifications[] = "external_modifications";
const char kBadPassword[] = "bad_password";
const char kCustomApnList[] = "custom_apn_list";
const char kCustomApnListV2[] = "custom_apn_list_v2";
const char kHasFixedHiddenNetworks[] =
    "metadata_store.has_fixed_hidden_networks";
const char kDayOfTrafficCountersAutoReset[] =
    "day_of_traffic_counters_auto_reset";
const char kUserTextMessageSuppressionState[] =
    "user_text_message_suppression_state";

constexpr base::TimeDelta kDefaultOverrideAge = base::Days(1);
// Wait two weeks before overwriting the creation timestamp for a given
// network.
constexpr base::TimeDelta kTwoWeeks = base::Days(14);

std::string GetPath(const std::string& guid, const std::string& subkey) {
  return base::StringPrintf("%s.%s", guid.c_str(), subkey.c_str());
}

base::Value::List CreateOrCloneListValue(const base::Value::List* list) {
  if (list)
    return list->Clone();

  return base::Value::List();
}

bool IsApnListValid(const base::Value::List& list) {
  for (const base::Value& apn : list) {
    if (!apn.is_dict())
      return false;

    if (!apn.GetDict().Find(::onc::cellular_apn::kAccessPointName))
      return false;
  }

  return true;
}

base::TimeDelta ComputeMigrationMinimumAge() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kHiddenNetworkMigrationAge)) {
    return kTwoWeeks;
  }

  int age_in_days = -1;
  const std::string ascii =
      command_line->GetSwitchValueASCII(switches::kHiddenNetworkMigrationAge);

  if (ascii.empty() || !base::StringToInt(ascii, &age_in_days) ||
      age_in_days < 0) {
    return kDefaultOverrideAge;
  }
  return base::Days(age_in_days);
}

}  // namespace

// static
void NetworkMetadataStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kNetworkMetadataPref);
  registry->RegisterBooleanPref(kHasFixedHiddenNetworks,
                                /*default_value=*/false);
}

NetworkMetadataStore::NetworkMetadataStore(
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    PrefService* profile_pref_service,
    PrefService* device_pref_service,
    bool is_enterprise_managed)
    : network_configuration_handler_(network_configuration_handler),
      network_connection_handler_(network_connection_handler),
      network_state_handler_(network_state_handler),
      managed_network_configuration_handler_(
          managed_network_configuration_handler),
      profile_pref_service_(profile_pref_service),
      device_pref_service_(device_pref_service),
      is_enterprise_managed_(is_enterprise_managed) {
  if (network_connection_handler_) {
    network_connection_handler_->AddObserver(this);
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->AddObserver(this);
  }
  if (network_state_handler_) {
    network_state_handler_observer_.Observe(network_state_handler_.get());
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

void NetworkMetadataStore::NetworkListChanged() {
  // Ensure that user networks have been loaded from Shill before querying.
  if (!network_state_handler_->IsProfileNetworksLoaded()) {
    has_profile_loaded_ = false;
    return;
  }

  if (has_profile_loaded_) {
    return;
  }

  has_profile_loaded_ = true;
  FixSyncedHiddenNetworks();
  LogHiddenNetworkAge();
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
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /*configured_only=*/true,
      /*visible_only=*/false, /*limit=*/0, &networks);
  for (const NetworkState* network : networks) {
    if (network->IsPrivate()) {
      continue;
    }

    SetIsCreatedByUser(network->guid());
  }
}

void NetworkMetadataStore::FixSyncedHiddenNetworks() {
  if (HasFixedHiddenNetworks()) {
    return;
  }

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /*configured_only=*/true,
      /*visible_only=*/false, /*limit=*/0, &networks);

  NET_LOG(EVENT) << "Updating networks from sync to disable HiddenSSID.";
  int total_count = 0;
  for (const NetworkState* network : networks) {
    if (!network->hidden_ssid()) {
      continue;
    }
    if (!GetIsConfiguredBySync(network->guid())) {
      continue;
    }

    total_count++;
    auto dict = base::Value::Dict().Set(shill::kWifiHiddenSsid, false);
    network_configuration_handler_->SetShillProperties(
        network->path(), std::move(dict), base::DoNothing(),
        base::BindOnce(&NetworkMetadataStore::OnDisableHiddenError,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  profile_pref_service_->SetBoolean(kHasFixedHiddenNetworks, true);
  base::UmaHistogramCounts1000("Network.Wifi.Synced.Hidden.Fixed", total_count);
}

void NetworkMetadataStore::LogHiddenNetworkAge() {
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::WiFi(), /*configured_only=*/true,
      /*visible_only=*/false, /*limit=*/0, &networks);

  for (const NetworkState* network : networks) {
    if (!network->hidden_ssid()) {
      continue;
    }
    base::TimeDelta timestamp = GetLastConnectedTimestamp(network->guid());
    if (!timestamp.is_zero()) {
      int days = base::Time::Now().ToDeltaSinceWindowsEpoch().InDays() -
                 timestamp.InDays();
      base::UmaHistogramCounts10000("Network.Shill.WiFi.Hidden.LastConnected",
                                    days);
    }
    base::UmaHistogramBoolean("Network.Shill.WiFi.Hidden.EverConnected",
                              !timestamp.is_zero());
  }
}

bool NetworkMetadataStore::HasFixedHiddenNetworks() {
  if (!profile_pref_service_) {
    // A user must be logged in to fix hidden networks.
    return true;
  }
  return profile_pref_service_->GetBoolean(kHasFixedHiddenNetworks);
}

void NetworkMetadataStore::OnDisableHiddenError(const std::string& error_name) {
  NET_LOG(EVENT) << "Failed to disable HiddenSSID on synced network. Error: "
                 << error_name;
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
  const base::Value::List* fields =
      GetListPref(network_guid, kExternalModifications);
  const bool contains_field = fields && base::Contains(*fields, field);
  if (GetIsCreatedByUser(network_guid)) {
    if (contains_field) {
      base::Value::List writeable_fields = CreateOrCloneListValue(fields);
      writeable_fields.EraseValue(base::Value(field));
      SetPref(network_guid, kExternalModifications,
              base::Value(std::move(writeable_fields)));
    }
  } else if (!contains_field) {
    base::Value::List writeable_fields = CreateOrCloneListValue(fields);
    writeable_fields.Append(field);
    SetPref(network_guid, kExternalModifications,
            base::Value(std::move(writeable_fields)));
  }
}

void NetworkMetadataStore::OnConfigurationModified(
    const std::string& service_path,
    const std::string& guid,
    const base::Value::Dict* set_properties) {
  if (!set_properties) {
    return;
  }

  SetPref(guid, kIsFromSync, base::Value(false));

  if (set_properties->Find(shill::kProxyConfigProperty)) {
    UpdateExternalModifications(guid, shill::kProxyConfigProperty);
  }
  if (set_properties->FindByDottedPath(
          base::StringPrintf("%s.%s", shill::kStaticIPConfigProperty,
                             shill::kNameServersProperty))) {
    UpdateExternalModifications(guid, shill::kNameServersProperty);
  }

  if (set_properties->Find(shill::kPassphraseProperty)) {
    // Only clear last connected if the passphrase changes.  Other settings
    // (autoconnect, dns, etc.) won't affect the ability to connect to a
    // network.
    SetPref(guid, kLastConnectedTimestampPref, base::Value(0));
    // Whichever user supplied the password is the "owner".
    SetIsCreatedByUser(guid);
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

  const base::Value::Dict& dict = pref_service->GetDict(kNetworkMetadataPref);
  if (!dict.contains(network_guid)) {
    return;
  }

  base::Value::Dict writeable_dict = dict.Clone();
  if (!writeable_dict.Remove(network_guid)) {
    return;
  }

  pref_service->SetDict(kNetworkMetadataPref, std::move(writeable_dict));
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

  return base::Milliseconds(timestamp->GetDouble());
}

void NetworkMetadataStore::SetLastConnectedTimestamp(
    const std::string& network_guid,
    const base::TimeDelta& timestamp) {
  double timestamp_f = timestamp.InMillisecondsF();
  SetPref(network_guid, kLastConnectedTimestampPref, base::Value(timestamp_f));
}

base::Time NetworkMetadataStore::UpdateAndRetrieveWiFiTimestamp(
    const std::string& network_guid) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);

  if (!network || network->GetNetworkTechnologyType() !=
                      NetworkState::NetworkTechnologyType::kWiFi) {
    return base::Time::Now().UTCMidnight();
  }

  const base::Value* creation_timestamp_pref =
      GetPref(network_guid, kCreationTimestamp);
  const base::Time current_timestamp = base::Time::Now().UTCMidnight();

  if (!creation_timestamp_pref) {
    SetPref(network_guid, kCreationTimestamp,
            base::Value(current_timestamp.InSecondsFSinceUnixEpoch()));
    return current_timestamp;
  }

  const base::Time creation_timestamp = base::Time::FromSecondsSinceUnixEpoch(
      creation_timestamp_pref->GetDouble());
  const base::TimeDelta minimum_age = ComputeMigrationMinimumAge();

  if (creation_timestamp + minimum_age <= current_timestamp) {
    SetPref(network_guid, kCreationTimestamp, base::Value(0.0));
    return base::Time::UnixEpoch();
  }
  return creation_timestamp;
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
  const base::Value::List* fields =
      GetListPref(network_guid, kExternalModifications);
  return fields && base::Contains(*fields, field);
}

bool NetworkMetadataStore::GetHasBadPassword(const std::string& network_guid) {
  const base::Value* has_bad_password = GetPref(network_guid, kBadPassword);

  // If the pref is not set, default to false.
  if (!has_bad_password) {
    return false;
  }

  return has_bad_password->GetBool();
}

void NetworkMetadataStore::SetCustomApnList(const std::string& network_guid,
                                            base::Value::List list) {
  if (ash::features::IsApnRevampEnabled()) {
    if (!IsApnListValid(list)) {
      NET_LOG(ERROR) << "network_guid: " << network_guid << std::endl
                     << "Invalid list passed to SetCustomApnList():" << list;
      return;
    }

    SetPref(network_guid, kCustomApnListV2, base::Value(std::move(list)));
    return;
  }

  SetPref(network_guid, kCustomApnList, base::Value(std::move(list)));
}

const base::Value::List* NetworkMetadataStore::GetCustomApnList(
    const std::string& network_guid) {
  if (ash::features::IsApnRevampEnabled()) {
    if (const base::Value* pref = GetPref(network_guid, kCustomApnListV2)) {
      return pref->GetIfList();
    }
    return nullptr;
  }

  if (const base::Value* pref = GetPref(network_guid, kCustomApnList)) {
    return pref->GetIfList();
  }
  return nullptr;
}

const base::Value::List* NetworkMetadataStore::GetPreRevampCustomApnList(
    const std::string& network_guid) {
  DCHECK(ash::features::IsApnRevampEnabled());
  if (const base::Value* pref = GetPref(network_guid, kCustomApnList)) {
    return pref->GetIfList();
  }
  return nullptr;
}

void NetworkMetadataStore::SetDayOfTrafficCountersAutoReset(
    const std::string& network_guid,
    const std::optional<int>& day) {
  auto value = day.has_value() ? base::Value(day.value()) : base::Value();
  SetPref(network_guid, kDayOfTrafficCountersAutoReset, std::move(value));
}

const base::Value* NetworkMetadataStore::GetDayOfTrafficCountersAutoReset(
    const std::string& network_guid) {
  return GetPref(network_guid, kDayOfTrafficCountersAutoReset);
}

void NetworkMetadataStore::SetSecureDnsTemplatesWithIdentifiersActive(
    bool active) {
  if (secure_dns_templates_with_identifiers_active_ == active) {
    return;
  }

  secure_dns_templates_with_identifiers_active_ = active;
  managed_network_configuration_handler_
      ->OnEnterpriseMonitoredWebPoliciesApplied();
}

void NetworkMetadataStore::SetReportXdrEventsEnabled(bool enabled) {
  if (report_xdr_events_enabled_ == enabled) {
    return;
  }

  report_xdr_events_enabled_ = enabled;
  managed_network_configuration_handler_
      ->OnEnterpriseMonitoredWebPoliciesApplied();
}

void NetworkMetadataStore::SetUserTextMessageSuppressionState(
    const std::string& network_guid,
    const UserTextMessageSuppressionState& state) {

  SetPref(network_guid, kUserTextMessageSuppressionState,
          base::Value(base::to_underlying(state)));
  CellularNetworkMetricsLogger::LogUserTextMessageSuppressionState(state);
}

UserTextMessageSuppressionState
NetworkMetadataStore::GetUserTextMessageSuppressionState(
    const std::string& network_guid) {

  const base::Value* state_value =
      GetPref(network_guid, kUserTextMessageSuppressionState);
  if (!state_value || !state_value->is_int()) {
    return UserTextMessageSuppressionState::kAllow;
  }

  if (base::to_underlying(UserTextMessageSuppressionState::kAllow) ==
      state_value->GetInt()) {
    return UserTextMessageSuppressionState::kAllow;
  } else if (base::to_underlying(UserTextMessageSuppressionState::kSuppress) ==
             state_value->GetInt()) {
    return UserTextMessageSuppressionState::kSuppress;
  }
  NOTREACHED_IN_MIGRATION();
  return UserTextMessageSuppressionState::kAllow;
}

void NetworkMetadataStore::SetPref(const std::string& network_guid,
                                   const std::string& key,
                                   base::Value value) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);

  if (network && network->IsPrivate() && profile_pref_service_) {
    base::Value::Dict profile_dict =
        profile_pref_service_->GetDict(kNetworkMetadataPref).Clone();
    profile_dict.SetByDottedPath(GetPath(network_guid, key), std::move(value));
    profile_pref_service_->SetDict(kNetworkMetadataPref,
                                   std::move(profile_dict));
    return;
  }

  base::Value::Dict device_dict =
      device_pref_service_->GetDict(kNetworkMetadataPref).Clone();
  device_dict.SetByDottedPath(GetPath(network_guid, key), std::move(value));
  device_pref_service_->SetDict(kNetworkMetadataPref, std::move(device_dict));
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
    const base::Value::Dict& profile_dict =
        profile_pref_service_->GetDict(kNetworkMetadataPref);
    const base::Value* value =
        profile_dict.FindByDottedPath(GetPath(network_guid, key));
    if (value)
      return value;
  }

  const base::Value::Dict& device_dict =
      device_pref_service_->GetDict(kNetworkMetadataPref);
  return device_dict.FindByDottedPath(GetPath(network_guid, key));
}

const base::Value::List* NetworkMetadataStore::GetListPref(
    const std::string& network_guid,
    const std::string& key) {
  const base::Value* pref = GetPref(network_guid, key);
  if (!pref)
    return nullptr;
  return pref->GetIfList();
}

void NetworkMetadataStore::AddObserver(NetworkMetadataObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkMetadataStore::RemoveObserver(NetworkMetadataObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
