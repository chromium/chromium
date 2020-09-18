// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "chromeos/network/policy_util.h"
#include "chromeos/network/prohibited_technologies_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/network/shill_property_util.h"
#include "chromeos/network/tether_constants.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

using GuidToPolicyMap = ManagedNetworkConfigurationHandler::GuidToPolicyMap;

// These are error strings used for error callbacks. None of these error
// messages are user-facing: they should only appear in logs.
const char kInvalidUserSettings[] = "InvalidUserSettings";
const char kNetworkAlreadyConfigured[] = "NetworkAlreadyConfigured";
const char kPoliciesNotInitialized[] = "PoliciesNotInitialized";
const char kProfileNotInitialized[] = "ProfileNotInitialized";
const char kUnconfiguredNetwork[] = "UnconfiguredNetwork";
const char kUnknownNetwork[] = "UnknownNetwork";

std::string ToDebugString(::onc::ONCSource source,
                          const std::string& userhash) {
  return source == ::onc::ONC_SOURCE_USER_POLICY
             ? ("user policy of " + userhash)
             : "device policy";
}

void InvokeErrorCallback(const std::string& service_path,
                         network_handler::ErrorCallback error_callback,
                         const std::string& error_name) {
  std::string error_msg = "ManagedConfig Error: " + error_name;
  NET_LOG(ERROR) << error_msg << " For: " << NetworkPathId(service_path);
  network_handler::RunErrorCallback(std::move(error_callback), service_path,
                                    error_name, error_msg);
}

void LogErrorWithDictAndCallCallback(
    base::OnceClosure callback,
    const base::Location& from_where,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  device_event_log::AddEntry(from_where.file_name(), from_where.line_number(),
                             device_event_log::LOG_TYPE_NETWORK,
                             device_event_log::LOG_LEVEL_ERROR, error_name);
  std::move(callback).Run();
}

const base::DictionaryValue* GetByGUID(const GuidToPolicyMap& policies,
                                       const std::string& guid) {
  auto it = policies.find(guid);
  if (it == policies.end())
    return nullptr;
  return it->second.get();
}

std::string GetStringFromDictionary(const base::Value& dict, const char* key) {
  const base::Value* v = dict.FindKey(key);
  return v ? v->GetString() : std::string();
}

bool MatchesExistingNetworkState(const base::DictionaryValue& properties,
                                 const NetworkState* network_state) {
  std::string type =
      GetStringFromDictionary(properties, ::onc::network_config::kType);
  if (network_util::TranslateONCTypeToShill(type) != network_state->type()) {
    NET_LOG(ERROR) << "Network type mismatch for: " << NetworkId(network_state)
                   << ", type: " << type
                   << " does not match: " << network_state->type();
    return false;
  }
  if (type != ::onc::network_type::kWiFi)
    return true;

  const base::Value* wifi = properties.FindKey(::onc::network_config::kWiFi);
  if (!wifi) {
    NET_LOG(ERROR) << "WiFi network configuration missing is WiFi properties: "
                   << NetworkId(network_state);
    return false;
  }
  // For WiFi networks ensure that Security and SSID match.
  std::string security = GetStringFromDictionary(*wifi, ::onc::wifi::kSecurity);
  if (network_util::TranslateONCSecurityToShill(security) !=
      network_state->security_class()) {
    NET_LOG(ERROR) << "Network security mismatch for: "
                   << NetworkId(network_state) << " security: " << security
                   << " does not match: " << network_state->security_class();
    return false;
  }
  std::string hex_ssid = GetStringFromDictionary(*wifi, ::onc::wifi::kHexSSID);
  if (hex_ssid != network_state->GetHexSsid()) {
    NET_LOG(ERROR) << "Network HexSSID mismatch for: "
                   << NetworkId(network_state) << " hex_ssid: " << hex_ssid
                   << " does not match: " << network_state->GetHexSsid();
    return false;
  }
  return true;
}

// Checks if |onc| is an unmanaged wifi network that has AutoConnect=true.
bool EnablesUnmanagedWifiAutoconnect(const base::DictionaryValue* onc) {
  const base::Value* type = onc->FindKeyOfType(::onc::network_config::kType,
                                               base::Value::Type::STRING);
  if (!type || type->GetString() != ::onc::network_type::kWiFi)
    return false;

  const base::Value* source = onc->FindKeyOfType(::onc::network_config::kSource,
                                                 base::Value::Type::STRING);
  if (!source ||
      source->GetString() == ::onc::network_config::kSourceDevicePolicy ||
      source->GetString() == ::onc::network_config::kSourceUserPolicy) {
    return false;
  }

  const base::Value* autoconnect = onc->FindPathOfType(
      {::onc::network_config::kWiFi, ::onc::wifi::kAutoConnect},
      base::Value::Type::BOOLEAN);
  return autoconnect && autoconnect->GetBool();
}

}  // namespace

struct ManagedNetworkConfigurationHandlerImpl::Policies {
  ~Policies();

  GuidToPolicyMap per_network_config;
  base::DictionaryValue global_network_config;
};

ManagedNetworkConfigurationHandlerImpl::Policies::~Policies() = default;

void ManagedNetworkConfigurationHandlerImpl::AddObserver(
    NetworkPolicyObserver* observer) {
  observers_.AddObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::RemoveObserver(
    NetworkPolicyObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::GetManagedProperties(
    const std::string& userhash,
    const std::string& service_path,
    network_handler::PropertiesCallback callback) {
  if (!GetPoliciesForUser(userhash) || !GetPoliciesForUser(std::string())) {
    NET_LOG(ERROR) << "GetManagedProperties failed: "
                   << kPoliciesNotInitialized;
    std::move(callback).Run(service_path, base::nullopt,
                            kPoliciesNotInitialized);
    return;
  }
  NET_LOG(USER) << "GetManagedProperties: " << NetworkPathId(service_path);
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr(), PropertiesType::kManaged, userhash,
          std::move(callback)));
}

void ManagedNetworkConfigurationHandlerImpl::GetProperties(
    const std::string& userhash,
    const std::string& service_path,
    network_handler::PropertiesCallback callback) {
  NET_LOG(USER) << "GetProperties for: " << NetworkPathId(service_path);
  network_configuration_handler_->GetShillProperties(
      service_path,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr(), PropertiesType::kUnmanaged, userhash,
          std::move(callback)));
}

void ManagedNetworkConfigurationHandlerImpl::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& user_settings,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const NetworkState* state =
      network_state_handler_->GetNetworkStateFromServicePath(
          service_path, true /* configured_only */);
  if (!state) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kUnknownNetwork);
    return;
  }

  std::string guid = state->guid();
  DCHECK(!guid.empty());

  const std::string& profile_path = state->profile_path();
  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    // TODO(pneubeck): create an initial configuration in this case. As for
    // CreateConfiguration, user settings from older ChromeOS versions have to
    // be determined here.
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kUnconfiguredNetwork);
    return;
  }

  NET_LOG(DEBUG) << "Set Managed Properties for: "
                 << NetworkPathId(service_path)
                 << ". Profile: " << profile->ToDebugString();

  const Policies* policies = GetPoliciesForProfile(*profile);
  if (!policies) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kPoliciesNotInitialized);
    return;
  }

  // We need to ensure that required configuration properties (e.g. Type) are
  // included for ONC validation and translation to Shill properties.
  std::unique_ptr<base::DictionaryValue> user_settings_copy(
      user_settings.DeepCopy());
  user_settings_copy->SetKey(
      ::onc::network_config::kType,
      base::Value(network_util::TranslateShillTypeToONC(state->type())));
  user_settings_copy->MergeDictionary(&user_settings);

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  onc::Validator validator(false,  // Ignore unknown fields.
                           false,  // Ignore invalid recommended field names.
                           false,  // Ignore missing fields.
                           false,  // This ONC does not come from policy.
                           true);  // Log warnings.

  onc::Validator::Result validation_result;
  std::unique_ptr<base::DictionaryValue> validated_user_settings =
      validator.ValidateAndRepairObject(&onc::kNetworkConfigurationSignature,
                                        *user_settings_copy,
                                        &validation_result);
  if (validation_result == onc::Validator::INVALID) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kInvalidUserSettings);
    return;
  }
  if (validation_result == onc::Validator::VALID_WITH_WARNINGS)
    NET_LOG(USER) << "Validation of ONC user settings produced warnings.";

  // Don't allow AutoConnect=true for unmanaged wifi networks if
  // 'AllowOnlyPolicyNetworksToAutoconnect' policy is active.
  if (EnablesUnmanagedWifiAutoconnect(validated_user_settings.get()) &&
      AllowOnlyPolicyNetworksToAutoconnect()) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kInvalidUserSettings);
    return;
  }

  // Fill in HexSSID field from contents of SSID field if not set already.
  onc::FillInHexSSIDFieldsInOncObject(onc::kNetworkConfigurationSignature,
                                      validated_user_settings.get());

  const base::DictionaryValue* network_policy =
      GetByGUID(policies->per_network_config, guid);
  if (network_policy)
    NET_LOG(DEBUG) << "Configuration is managed: " << NetworkId(state);

  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      policy_util::CreateShillConfiguration(
          *profile, guid, &policies->global_network_config, network_policy,
          validated_user_settings.get()));

  SetShillProperties(service_path, std::move(shill_dictionary),
                     std::move(callback), std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::SetManagedActiveProxyValues(
    const std::string& guid,
    base::DictionaryValue* dictionary) {
  DCHECK(ui_proxy_config_service_);
  const std::string proxy_settings_key = ::onc::network_config::kProxySettings;
  base::Value* proxy_settings = dictionary->FindKeyOfType(
      proxy_settings_key, base::Value::Type::DICTIONARY);

  if (!proxy_settings) {
    proxy_settings = dictionary->SetKey(
        proxy_settings_key, base::Value(base::Value::Type::DICTIONARY));
  }
  ui_proxy_config_service_->MergeEnforcedProxyConfig(guid, proxy_settings);

  if (proxy_settings->DictEmpty())
    dictionary->RemoveKey(proxy_settings_key);
}

void ManagedNetworkConfigurationHandlerImpl::SetShillProperties(
    const std::string& service_path,
    std::unique_ptr<base::DictionaryValue> shill_dictionary,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  network_configuration_handler_->SetShillProperties(
      service_path, *shill_dictionary, std::move(callback),
      std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfiguration(
    const std::string& userhash,
    const base::DictionaryValue& properties,
    network_handler::ServiceResultCallback callback,
    network_handler::ErrorCallback error_callback) const {
  std::string guid =
      GetStringFromDictionary(properties, ::onc::network_config::kGUID);
  const NetworkState* network_state = nullptr;
  if (!guid.empty())
    network_state = network_state_handler_->GetNetworkStateFromGuid(guid);
  if (network_state) {
    NET_LOG(USER) << "CreateConfiguration for: " << NetworkId(network_state);
  } else {
    std::string type =
        GetStringFromDictionary(properties, ::onc::network_config::kType);
    NET_LOG(USER) << "Create new network configuration, Type: " << type;
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  onc::Validator validator(false,   // Ignore unknown fields.
                           false,   // Ignore invalid recommended field names.
                           false,   // Ignore missing fields.
                           false,   // This ONC does not come from policy.
                           false);  // Don't log warnings.

  onc::Validator::Result validation_result;
  std::unique_ptr<base::DictionaryValue> validated_properties =
      validator.ValidateAndRepairObject(&onc::kNetworkConfigurationSignature,
                                        properties, &validation_result);

  if (validation_result == onc::Validator::INVALID) {
    InvokeErrorCallback("", std::move(error_callback), kInvalidUserSettings);
    return;
  }

  if (validation_result == onc::Validator::VALID_WITH_WARNINGS)
    NET_LOG(DEBUG) << "Validation of ONC user settings produced warnings.";

  // Fill in HexSSID field from contents of SSID field if not set already - this
  // is required to properly match the configuration against existing policies.
  onc::FillInHexSSIDFieldsInOncObject(onc::kNetworkConfigurationSignature,
                                      validated_properties.get());

  // Make sure the network is not configured through a user policy.
  const Policies* policies = nullptr;
  if (!userhash.empty()) {
    policies = GetPoliciesForUser(userhash);
    if (!policies) {
      InvokeErrorCallback("", std::move(error_callback),
                          kPoliciesNotInitialized);
      return;
    }

    if (policy_util::FindMatchingPolicy(policies->per_network_config,
                                        *validated_properties)) {
      InvokeErrorCallback("", std::move(error_callback),
                          kNetworkAlreadyConfigured);
      return;
    }
  }

  // Make user the network is not configured through a device policy.
  policies = GetPoliciesForUser("");
  if (!policies) {
    InvokeErrorCallback("", std::move(error_callback), kPoliciesNotInitialized);
    return;
  }

  if (policy_util::FindMatchingPolicy(policies->per_network_config,
                                      *validated_properties)) {
    InvokeErrorCallback("", std::move(error_callback),
                        kNetworkAlreadyConfigured);
    return;
  }

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    InvokeErrorCallback("", std::move(error_callback), kProfileNotInitialized);
    return;
  }

  // If a GUID was provided, verify that the new configuraiton matches an
  // existing NetworkState for an unconfigured (i.e. visible) network.
  // Requires HexSSID to be set first for comparing SSIDs.
  if (!guid.empty()) {
    // |network_state| can by null if a network went out of range or was
    // forgotten while the UI is open. Configuration should succeed and the GUID
    // can be reused.
    if (network_state) {
      if (!MatchesExistingNetworkState(*validated_properties, network_state)) {
        InvokeErrorCallback(network_state->path(), std::move(error_callback),
                            kNetworkAlreadyConfigured);
        return;
      } else if (!network_state->profile_path().empty()) {
        // Can occur after an invalid password or with multiple config UIs open.
        // Configuration should succeed, so just log an event.
        NET_LOG(EVENT) << "Reconfiguring network: " << NetworkId(network_state)
                       << " Profile: " << network_state->profile_path();
      }
    }
  } else {
    guid = base::GenerateGUID();
  }

  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      policy_util::CreateShillConfiguration(*profile, guid,
                                            nullptr,  // no global policy
                                            nullptr,  // no network policy
                                            validated_properties.get()));

  network_configuration_handler_->CreateShillConfiguration(
      *shill_dictionary, std::move(callback), std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::RemoveConfiguration(
    const std::string& service_path,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) const {
  network_configuration_handler_->RemoveConfiguration(
      service_path,
      base::BindRepeating(
          &ManagedNetworkConfigurationHandlerImpl::CanRemoveNetworkConfig,
          base::Unretained(this)),
      std::move(callback), std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::
    RemoveConfigurationFromCurrentProfile(
        const std::string& service_path,
        base::OnceClosure callback,
        network_handler::ErrorCallback error_callback) const {
  network_configuration_handler_->RemoveConfigurationFromCurrentProfile(
      service_path, std::move(callback), std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::SetPolicy(
    ::onc::ONCSource onc_source,
    const std::string& userhash,
    const base::ListValue& network_configs_onc,
    const base::DictionaryValue& global_network_config) {
  VLOG(1) << "Setting policies from: " << ToDebugString(onc_source, userhash);

  // |userhash| must be empty for device policies.
  DCHECK(onc_source != ::onc::ONC_SOURCE_DEVICE_POLICY || userhash.empty());
  Policies* policies = nullptr;
  if (base::Contains(policies_by_user_, userhash)) {
    policies = policies_by_user_[userhash].get();
  } else {
    policies = new Policies;
    policies_by_user_[userhash] = base::WrapUnique(policies);
  }

  policies->global_network_config.MergeDictionary(&global_network_config);

  // Update prohibited technologies.
  const base::ListValue* prohibited_list = nullptr;
  if (policies->global_network_config.GetListWithoutPathExpansion(
          ::onc::global_network_config::kDisableNetworkTypes,
          &prohibited_list) &&
      prohibited_technologies_handler_) {
    // Prohibited technologies are only allowed in device policy.
    DCHECK_EQ(::onc::ONC_SOURCE_DEVICE_POLICY, onc_source);

    prohibited_technologies_handler_->SetProhibitedTechnologies(
        prohibited_list);
  }

  GuidToPolicyMap old_per_network_config;
  policies->per_network_config.swap(old_per_network_config);

  // This stores all GUIDs of policies that have changed or are new.
  std::set<std::string> modified_policies;

  for (base::ListValue::const_iterator it = network_configs_onc.begin();
       it != network_configs_onc.end(); ++it) {
    const base::DictionaryValue* network = nullptr;
    it->GetAsDictionary(&network);
    DCHECK(network);

    std::string guid;
    network->GetStringWithoutPathExpansion(::onc::network_config::kGUID, &guid);
    DCHECK(!guid.empty());

    if (policies->per_network_config.count(guid) > 0) {
      NET_LOG(ERROR) << "ONC from: " << ToDebugString(onc_source, userhash)
                     << " Contains multiple entries for the same guid: "
                     << guid;
    }
    base::DictionaryValue* new_entry = network->DeepCopy();
    policies->per_network_config[guid] = base::WrapUnique(new_entry);

    base::DictionaryValue* old_entry = old_per_network_config[guid].get();
    if (!old_entry || !old_entry->Equals(new_entry))
      modified_policies.insert(guid);
  }

  old_per_network_config.clear();
  ApplyOrQueuePolicies(userhash, &modified_policies);
  for (auto& observer : observers_)
    observer.PoliciesChanged(userhash);
}

bool ManagedNetworkConfigurationHandlerImpl::IsAnyPolicyApplicationRunning()
    const {
  return !policy_applicators_.empty() || !queued_modified_policies_.empty();
}

bool ManagedNetworkConfigurationHandlerImpl::ApplyOrQueuePolicies(
    const std::string& userhash,
    std::set<std::string>* modified_policies) {
  DCHECK(modified_policies);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    VLOG(1) << "The relevant Shill profile isn't initialized yet, postponing "
            << "policy application.";
    // OnProfileAdded will apply all policies for this userhash.
    return false;
  }

  if (base::Contains(policy_applicators_, userhash)) {
    // A previous policy application is still running. Queue the modified
    // policies.
    // Note, even if |modified_policies| is empty, this means that a policy
    // application will be queued.
    queued_modified_policies_[userhash].insert(modified_policies->begin(),
                                               modified_policies->end());
    VLOG(1) << "Previous PolicyApplicator still running. Postponing policy "
               "application.";
    return false;
  }

  const Policies* policies = policies_by_user_[userhash].get();
  DCHECK(policies);

  PolicyApplicator* applicator = new PolicyApplicator(
      *profile, policies->per_network_config, policies->global_network_config,
      this, modified_policies);
  policy_applicators_[userhash] = base::WrapUnique(applicator);
  applicator->Run();
  return true;
}

void ManagedNetworkConfigurationHandlerImpl::set_ui_proxy_config_service(
    UIProxyConfigService* ui_proxy_config_service) {
  ui_proxy_config_service_ = ui_proxy_config_service;
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileAdded(
    const NetworkProfile& profile) {
  VLOG(1) << "Adding profile: " << profile.ToDebugString();

  const Policies* policies = GetPoliciesForProfile(profile);
  if (!policies) {
    VLOG(1) << "The relevant policy is not initialized, "
            << "postponing policy application.";
    // See SetPolicy.
    return;
  }

  std::set<std::string> policy_guids;
  for (auto it = policies->per_network_config.begin();
       it != policies->per_network_config.end(); ++it) {
    policy_guids.insert(it->first);
  }

  const bool started_policy_application =
      ApplyOrQueuePolicies(profile.userhash, &policy_guids);
  DCHECK(started_policy_application);
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileRemoved(
    const NetworkProfile& profile) {
  // Nothing to do in this case.
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfigurationFromPolicy(
    const base::DictionaryValue& shill_properties,
    base::OnceClosure callback) {
  base::RepeatingClosure adapted_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  network_configuration_handler_->CreateShillConfiguration(
      shill_properties,
      base::BindRepeating(
          &ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork,
          weak_ptr_factory_.GetWeakPtr(), adapted_callback),
      base::BindRepeating(&LogErrorWithDictAndCallCallback, adapted_callback,
                          FROM_HERE));
}

void ManagedNetworkConfigurationHandlerImpl::
    UpdateExistingConfigurationWithPropertiesFromPolicy(
        const base::DictionaryValue& existing_properties,
        const base::DictionaryValue& new_properties,
        base::OnceClosure callback) {
  base::RepeatingClosure adapted_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  base::DictionaryValue shill_properties;

  std::string profile;
  existing_properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                    &profile);
  if (profile.empty()) {
    NET_LOG(ERROR) << "Missing profile property: "
                   << shill_property_util::GetNetworkIdFromProperties(
                          existing_properties);
    return;
  }
  shill_properties.SetKey(shill::kProfileProperty, base::Value(profile));

  if (!shill_property_util::CopyIdentifyingProperties(
          existing_properties, true /* properties were read from Shill */,
          &shill_properties)) {
    NET_LOG(ERROR) << "Missing identifying properties",
        shill_property_util::GetNetworkIdFromProperties(existing_properties);
  }

  shill_properties.MergeDictionary(&new_properties);

  network_configuration_handler_->CreateShillConfiguration(
      shill_properties,
      base::BindRepeating(
          &ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork,
          weak_ptr_factory_.GetWeakPtr(), adapted_callback),
      base::BindRepeating(&LogErrorWithDictAndCallCallback, adapted_callback,
                          FROM_HERE));
}

void ManagedNetworkConfigurationHandlerImpl::OnPoliciesApplied(
    const NetworkProfile& profile) {
  const std::string& userhash = profile.userhash;
  VLOG(1) << "Policy application for user '" << userhash << "' finished.";

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, policy_applicators_[userhash].release());
  policy_applicators_.erase(userhash);

  if (base::Contains(queued_modified_policies_, userhash)) {
    std::set<std::string> modified_policies;
    queued_modified_policies_[userhash].swap(modified_policies);
    // Remove |userhash| from the queue.
    queued_modified_policies_.erase(userhash);
    ApplyOrQueuePolicies(userhash, &modified_policies);
  } else {
    if (userhash.empty())
      device_policy_applied_ = true;
    else
      user_policy_applied_ = true;

    if (device_policy_applied_ && user_policy_applied_) {
      network_state_handler_->UpdateBlockedWifiNetworks(
          AllowOnlyPolicyNetworksToConnect(),
          AllowOnlyPolicyNetworksToConnectIfAvailable(), GetBlockedHexSSIDs());
    }

    for (auto& observer : observers_)
      observer.PoliciesApplied(userhash);
  }
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::FindPolicyByGUID(
    const std::string userhash,
    const std::string& guid,
    ::onc::ONCSource* onc_source) const {
  *onc_source = ::onc::ONC_SOURCE_NONE;

  if (!userhash.empty()) {
    const Policies* user_policies = GetPoliciesForUser(userhash);
    if (user_policies) {
      const base::DictionaryValue* policy =
          GetByGUID(user_policies->per_network_config, guid);
      if (policy) {
        *onc_source = ::onc::ONC_SOURCE_USER_POLICY;
        return policy;
      }
    }
  }

  const Policies* device_policies = GetPoliciesForUser(std::string());
  if (device_policies) {
    const base::DictionaryValue* policy =
        GetByGUID(device_policies->per_network_config, guid);
    if (policy) {
      *onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
      return policy;
    }
  }

  return nullptr;
}

const GuidToPolicyMap*
ManagedNetworkConfigurationHandlerImpl::GetNetworkConfigsFromPolicy(
    const std::string& userhash) const {
  const Policies* policies = GetPoliciesForUser(userhash);
  if (!policies)
    return nullptr;

  return &policies->per_network_config;
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::GetGlobalConfigFromPolicy(
    const std::string& userhash) const {
  const Policies* policies = GetPoliciesForUser(userhash);
  if (!policies)
    return nullptr;

  return &policies->global_network_config;
}

const base::DictionaryValue*
ManagedNetworkConfigurationHandlerImpl::FindPolicyByGuidAndProfile(
    const std::string& guid,
    const std::string& profile_path,
    ::onc::ONCSource* onc_source) const {
  if (profile_path.empty())
    return nullptr;

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    NET_LOG(ERROR) << "Profile path unknown:" << profile_path
                   << " For: " << NetworkGuidId(guid);
    return nullptr;
  }

  const Policies* policies = GetPoliciesForProfile(*profile);
  if (!policies)
    return nullptr;

  const base::DictionaryValue* policy =
      GetByGUID(policies->per_network_config, guid);
  if (policy && onc_source) {
    *onc_source = (profile->userhash.empty() ? ::onc::ONC_SOURCE_DEVICE_POLICY
                                             : ::onc::ONC_SOURCE_USER_POLICY);
  }
  return policy;
}

bool ManagedNetworkConfigurationHandlerImpl::IsNetworkConfiguredByPolicy(
    const std::string& guid,
    const std::string& profile_path) const {
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_UNKNOWN;
  return FindPolicyByGUID(guid, profile_path, &onc_source) != nullptr;
}

bool ManagedNetworkConfigurationHandlerImpl::CanRemoveNetworkConfig(
    const std::string& guid,
    const std::string& profile_path) const {
  return !IsNetworkConfiguredByPolicy(guid, profile_path);
}

bool ManagedNetworkConfigurationHandlerImpl::AllowOnlyPolicyNetworksToConnect()
    const {
  const base::DictionaryValue* global_network_config =
      GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (!global_network_config)
    return false;

  const base::Value* managed_only_value = global_network_config->FindKeyOfType(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
      base::Value::Type::BOOLEAN);
  return managed_only_value && managed_only_value->GetBool();
}

bool ManagedNetworkConfigurationHandlerImpl::
    AllowOnlyPolicyNetworksToConnectIfAvailable() const {
  const base::DictionaryValue* global_network_config =
      GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (!global_network_config)
    return false;

  // Check if policy is enabled.
  const base::Value* available_only_value =
      global_network_config->FindKeyOfType(
          ::onc::global_network_config::
              kAllowOnlyPolicyNetworksToConnectIfAvailable,
          base::Value::Type::BOOLEAN);
  return available_only_value && available_only_value->GetBool();
}

bool ManagedNetworkConfigurationHandlerImpl::
    AllowOnlyPolicyNetworksToAutoconnect() const {
  const base::DictionaryValue* global_network_config =
      GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (!global_network_config)
    return false;

  const base::Value* autoconnect_value = global_network_config->FindKeyOfType(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      base::Value::Type::BOOLEAN);
  return autoconnect_value && autoconnect_value->GetBool();
}

std::vector<std::string>
ManagedNetworkConfigurationHandlerImpl::GetBlockedHexSSIDs() const {
  const base::DictionaryValue* global_network_config =
      GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (!global_network_config)
    return std::vector<std::string>();

  const base::Value* blocked_value = global_network_config->FindKeyOfType(
      ::onc::global_network_config::kBlockedHexSSIDs, base::Value::Type::LIST);
  if (!blocked_value)
    return std::vector<std::string>();

  std::vector<std::string> blocked_hex_ssids;
  for (const base::Value& entry : blocked_value->GetList())
    blocked_hex_ssids.push_back(entry.GetString());
  return blocked_hex_ssids;
}

const ManagedNetworkConfigurationHandlerImpl::Policies*
ManagedNetworkConfigurationHandlerImpl::GetPoliciesForUser(
    const std::string& userhash) const {
  UserToPoliciesMap::const_iterator it = policies_by_user_.find(userhash);
  if (it == policies_by_user_.end())
    return nullptr;
  return it->second.get();
}

const ManagedNetworkConfigurationHandlerImpl::Policies*
ManagedNetworkConfigurationHandlerImpl::GetPoliciesForProfile(
    const NetworkProfile& profile) const {
  DCHECK(profile.type() != NetworkProfile::TYPE_SHARED ||
         profile.userhash.empty());
  return GetPoliciesForUser(profile.userhash);
}

ManagedNetworkConfigurationHandlerImpl::
    ManagedNetworkConfigurationHandlerImpl() {
  CHECK(base::ThreadTaskRunnerHandle::IsSet());
}

ManagedNetworkConfigurationHandlerImpl::
    ~ManagedNetworkConfigurationHandlerImpl() {
  if (network_profile_handler_)
    network_profile_handler_->RemoveObserver(this);
}

void ManagedNetworkConfigurationHandlerImpl::Init(
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkDeviceHandler* network_device_handler,
    ProhibitedTechnologiesHandler* prohibited_technologies_handler) {
  network_state_handler_ = network_state_handler;
  network_profile_handler_ = network_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_device_handler_ = network_device_handler;
  if (network_profile_handler_)
    network_profile_handler_->AddObserver(this);
  prohibited_technologies_handler_ = prohibited_technologies_handler;
}

void ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork(
    base::OnceClosure callback,
    const std::string& service_path,
    const std::string& guid) {
  DCHECK(!service_path.empty());

  // When this is called, the policy has been fully applied and is reflected in
  // NetworkStateHandler, so it is safe to notify obserers.
  // Notifying observers is the last step of policy application to
  // |service_path|.
  for (auto& observer : observers_)
    observer.PolicyAppliedToNetwork(service_path);

  // Inform the caller that has requested policy application that it has
  // finished.
  std::move(callback).Run();
}

// Get{Managed}Properties helpers

void ManagedNetworkConfigurationHandlerImpl::GetDeviceStateProperties(
    const std::string& service_path,
    base::Value* properties) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    NET_LOG(ERROR) << "GetDeviceStateProperties: no network for: "
                   << NetworkPathId(service_path);
    return;
  }
  if (!network->IsConnectedState())
    return;  // No (non saved) IP Configs for non connected networks.

  const DeviceState* device_state =
      network->device_path().empty()
          ? nullptr
          : network_state_handler_->GetDeviceState(network->device_path());

  // Get the hardware MAC address from the DeviceState.
  if (device_state && !device_state->mac_address().empty()) {
    properties->SetKey(shill::kAddressProperty,
                       base::Value(device_state->mac_address()));
  }

  // Get the IPConfig properties from the device and store them in "IPConfigs"
  // (plural) in the properties dictionary. (Note: Shill only provides a single
  // "IPConfig" property for a network service, but a consumer of this API may
  // want information about all ipv4 and ipv6 IPConfig properties.
  base::Value ip_configs(base::Value::Type::LIST);

  if (!device_state || device_state->ip_configs().empty()) {
    // Shill may not provide IPConfigs for external Cellular devices/dongles
    // (https://crbug.com/739314) or VPNs, so build a dictionary of ipv4
    // properties from cached NetworkState properties .
    NET_LOG(DEBUG)
        << "GetDeviceStateProperties: Setting IPv4 properties from network: "
        << NetworkId(network);
    if (network->ipv4_config())
      ip_configs.Append(network->ipv4_config()->Clone());
  } else {
    // Convert the DeviceState IPConfigs dictionary to a ListValue.
    for (const auto iter : device_state->ip_configs().DictItems())
      ip_configs.Append(iter.second.Clone());
  }
  if (!ip_configs.GetList().empty()) {
    properties->SetKey(shill::kIPConfigsProperty, std::move(ip_configs));
  }
}

void ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback(
    PropertiesType properties_type,
    const std::string& userhash,
    network_handler::PropertiesCallback callback,
    const std::string& service_path,
    base::Optional<base::Value> shill_properties) {
  if (!shill_properties) {
    SendProperties(properties_type, userhash, service_path, std::move(callback),
                   base::nullopt);
    return;
  }

  const std::string* guid =
      shill_properties->FindStringKey(shill::kGuidProperty);
  if (!guid || guid->empty()) {
    // Unmanaged networks are assigned a GUID in NetworkState. Provide this
    // value in the ONC dictionary.
    const NetworkState* state =
        network_state_handler_->GetNetworkState(service_path);
    if (state && !state->guid().empty()) {
      shill_properties->SetKey(shill::kGuidProperty,
                               base::Value(state->guid()));
    } else {
      NET_LOG(ERROR) << "Network has no GUID specified: "
                     << NetworkPathId(service_path);
    }
  }

  const std::string* type =
      shill_properties->FindStringKey(shill::kTypeProperty);
  // Add any associated DeviceState properties.
  GetDeviceStateProperties(service_path, &shill_properties.value());

  // Only request additional Device properties for Cellular networks with a
  // valid device.
  std::string* device_path =
      shill_properties->FindStringKey(shill::kDeviceProperty);
  if (!network_device_handler_ || *type != shill::kTypeCellular ||
      !device_path || device_path->empty()) {
    SendProperties(properties_type, userhash, service_path, std::move(callback),
                   std::move(shill_properties));
    return;
  }

  // Request the device properties. On success or failure pass (a possibly
  // modified) |shill_properties| to |send_callback|.
  network_device_handler_->GetDeviceProperties(
      *device_path,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::OnGetDeviceProperties,
          weak_ptr_factory_.GetWeakPtr(), properties_type, userhash,
          service_path, std::move(callback), std::move(shill_properties)));
}

void ManagedNetworkConfigurationHandlerImpl::OnGetDeviceProperties(
    PropertiesType properties_type,
    const std::string& userhash,
    const std::string& service_path,
    network_handler::PropertiesCallback callback,
    base::Optional<base::Value> network_properties,
    const std::string& device_path,
    base::Optional<base::Value> device_properties) {
  DCHECK(network_properties);
  if (!device_properties) {
    NET_LOG(ERROR) << "Error getting device properties: "
                   << NetworkPathId(service_path);
  } else {
    // Create a "Device" dictionary in |network_properties|.
    network_properties->SetKey(shill::kDeviceProperty,
                               std::move(*device_properties));
  }
  SendProperties(properties_type, userhash, service_path, std::move(callback),
                 std::move(network_properties));
}

void ManagedNetworkConfigurationHandlerImpl::SendProperties(
    PropertiesType properties_type,
    const std::string& userhash,
    const std::string& service_path,
    network_handler::PropertiesCallback callback,
    base::Optional<base::Value> shill_properties) {
  auto get_name = [](PropertiesType properties_type) {
    switch (properties_type) {
      case PropertiesType::kUnmanaged:
        return "GetProperties";
      case PropertiesType::kManaged:
        return "GetManagedProperties";
    }
    return "";
  };

  if (!shill_properties) {
    NET_LOG(ERROR) << get_name(properties_type) << " Failed.";
    std::move(callback).Run(service_path, base::nullopt,
                            network_handler::kDBusFailedError);
    return;
  }
  const std::string* guid =
      shill_properties->FindStringKey(shill::kGuidProperty);
  if (!guid) {
    NET_LOG(ERROR) << get_name(properties_type) << " Missing GUID.";
    std::move(callback).Run(service_path, base::nullopt, kUnknownNetwork);
    return;
  }

  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  ::onc::ONCSource onc_source;
  FindPolicyByGUID(userhash, *guid, &onc_source);
  std::unique_ptr<base::DictionaryValue> onc_network(
      onc::TranslateShillServiceToONCPart(
          base::Value::AsDictionaryValue(*shill_properties), onc_source,
          &onc::kNetworkWithStateSignature, network_state));

  if (properties_type == PropertiesType::kUnmanaged) {
    std::move(callback).Run(service_path,
                            base::make_optional(base::Value::FromUniquePtrValue(
                                std::move(onc_network))),
                            base::nullopt);
    return;
  }

  const std::string* profile_path =
      shill_properties->FindStringKey(shill::kProfileProperty);
  const NetworkProfile* profile =
      profile_path && network_profile_handler_
          ? network_profile_handler_->GetProfileForPath(*profile_path)
          : nullptr;
  if (!profile && !(network_state && network_state->IsNonProfileType())) {
    // Visible but unsaved (not known) networks will not have a profile.
    NET_LOG(DEBUG) << "No profile for: " << NetworkId(network_state)
                   << " Profile path: " << profile_path;
  }

  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(
          base::Value::AsDictionaryValue(*shill_properties));

  const base::DictionaryValue* user_settings = nullptr;

  if (ui_data && profile) {
    user_settings = ui_data->GetUserSettingsDictionary();
  } else if (profile) {
    NET_LOG(DEBUG) << "Network contains empty or invalid UIData: "
                   << NetworkId(network_state);
    // TODO(pneubeck): add a conversion of user configured entries of old
    // ChromeOS versions. We will have to use a heuristic to determine which
    // properties _might_ be user configured.
  }

  const base::DictionaryValue* network_policy = nullptr;
  const base::DictionaryValue* global_policy = nullptr;
  if (profile) {
    const Policies* policies = GetPoliciesForProfile(*profile);
    if (!policies) {
      NET_LOG(ERROR) << "GetManagedProperties failed: "
                     << kPoliciesNotInitialized;
      std::move(callback).Run(service_path, base::nullopt,
                              kPoliciesNotInitialized);
      return;
    }
    if (!guid->empty())
      network_policy = GetByGUID(policies->per_network_config, *guid);
    global_policy = &policies->global_network_config;
  }

  std::unique_ptr<base::DictionaryValue> augmented_properties(
      policy_util::CreateManagedONC(global_policy, network_policy,
                                    user_settings, onc_network.get(), profile));
  SetManagedActiveProxyValues(*guid, augmented_properties.get());
  std::move(callback).Run(service_path,
                          base::make_optional(base::Value::FromUniquePtrValue(
                              std::move(augmented_properties))),
                          base::nullopt);
}

}  // namespace chromeos
