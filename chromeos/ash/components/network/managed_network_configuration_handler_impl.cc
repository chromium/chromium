// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/cellular_policy_handler.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/metrics/esim_policy_login_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/wifi_network_metrics_helper.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/onc/onc_merger.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/components/onc/onc_validator.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kEmptyServicePath[] = "/";

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
  NET_LOG(ERROR) << "ManagedConfig Error: " << error_name
                 << " For: " << NetworkPathId(service_path);
  network_handler::RunErrorCallback(std::move(error_callback), error_name);
}

void LogErrorWithDictAndCallCallback(base::OnceClosure callback,
                                     const base::Location& from_where,
                                     const std::string& error_name) {
  device_event_log::AddEntry(from_where.file_name(), from_where.line_number(),
                             device_event_log::LOG_TYPE_NETWORK,
                             device_event_log::LOG_LEVEL_ERROR, error_name);
  std::move(callback).Run();
}

void OnResetDnsPropertiesFailure(const std::string& error_name) {
  NET_LOG(ERROR) << "Failed to clear DNS Configurations, error name: "
                 << error_name;
}

void OnSetCustomApnFailure(const std::string& error_name) {
  NET_LOG(ERROR) << "Failed to set custom APNs, error name: " << error_name;
}

std::string GetStringFromDictionary(const base::Value::Dict& dict,
                                    const char* key) {
  const std::string* v = dict.FindString(key);
  return v ? *v : std::string();
}

bool MatchesExistingNetworkState(const base::Value::Dict& properties,
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

  const base::Value::Dict* wifi =
      properties.FindDict(::onc::network_config::kWiFi);
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
bool EnablesUnmanagedWifiAutoconnect(const base::Value::Dict& onc_dict) {
  const std::string* type = onc_dict.FindString(::onc::network_config::kType);
  if (!type || *type != ::onc::network_type::kWiFi) {
    return false;
  }

  const std::string* source =
      onc_dict.FindString(::onc::network_config::kSource);
  if (!source || *source == ::onc::network_config::kSourceDevicePolicy ||
      *source == ::onc::network_config::kSourceUserPolicy) {
    return false;
  }

  const base::Value::Dict* wifi_config =
      onc_dict.FindDict(::onc::network_config::kWiFi);
  if (!wifi_config) {
    return false;
  }

  std::optional<bool> autoconnect =
      wifi_config->FindBool(::onc::wifi::kAutoConnect);
  return autoconnect.has_value() && autoconnect.value();
}

}  // namespace

ManagedNetworkConfigurationHandlerImpl::PolicyApplicationInfo::
    PolicyApplicationInfo() = default;
ManagedNetworkConfigurationHandlerImpl::PolicyApplicationInfo::
    ~PolicyApplicationInfo() = default;

ManagedNetworkConfigurationHandlerImpl::PolicyApplicationInfo::
    PolicyApplicationInfo(PolicyApplicationInfo&& other) = default;

ManagedNetworkConfigurationHandlerImpl::PolicyApplicationInfo&
ManagedNetworkConfigurationHandlerImpl::PolicyApplicationInfo::operator=(
    PolicyApplicationInfo&& other) = default;

void ManagedNetworkConfigurationHandlerImpl::AddObserver(
    NetworkPolicyObserver* observer) {
  observers_.AddObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::RemoveObserver(
    NetworkPolicyObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ManagedNetworkConfigurationHandlerImpl::HasObserver(
    NetworkPolicyObserver* observer) const {
  return observers_.HasObserver(observer);
}

void ManagedNetworkConfigurationHandlerImpl::Shutdown() {
  if (did_shutdown_)
    return;  // May get called twice in tests.

  did_shutdown_ = true;
  if (network_profile_handler_ && network_profile_handler_->HasObserver(this))
    network_profile_handler_->RemoveObserver(this);
  network_profile_handler_ = nullptr;

  for (auto& observer : observers_)
    observer.OnManagedNetworkConfigurationHandlerShuttingDown();
}

void ManagedNetworkConfigurationHandlerImpl::GetManagedProperties(
    const std::string& userhash,
    const std::string& service_path,
    network_handler::PropertiesCallback callback) {
  if (!GetPoliciesForUser(userhash) || !GetPoliciesForUser(std::string())) {
    NET_LOG(ERROR) << "GetManagedProperties failed: "
                   << kPoliciesNotInitialized;
    std::move(callback).Run(service_path, std::nullopt,
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
    const base::Value::Dict& user_settings,
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

  const ProfilePolicies* policies = GetPoliciesForProfile(*profile);
  if (!policies) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kPoliciesNotInitialized);
    return;
  }

  // We need to ensure that required configuration properties (e.g. Type) are
  // included for ONC validation and translation to Shill properties.
  base::Value::Dict user_settings_copy = user_settings.Clone();
  if (!user_settings_copy.contains(::onc::network_config::kType)) {
    user_settings_copy.Set(
        ::onc::network_config::kType,
        network_util::TranslateShillTypeToONC(state->type()));
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  chromeos::onc::Validator validator(
      /*error_on_unknown_field=*/false,
      /*error_on_wrong_recommended=*/false,
      /*error_on_missing_field=*/false,
      /*managed_onc=*/false,
      /*log_warnings=*/true);

  chromeos::onc::Validator::Result validation_result;
  std::optional<base::Value::Dict> validated_user_settings =
      validator.ValidateAndRepairObject(
          &chromeos::onc::kNetworkConfigurationSignature, user_settings_copy,
          &validation_result);
  if (validation_result == chromeos::onc::Validator::INVALID) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kInvalidUserSettings);
    return;
  }
  if (validation_result == chromeos::onc::Validator::VALID_WITH_WARNINGS) {
    NET_LOG(USER) << "Validation of ONC user settings produced warnings.";
  }

  // Don't allow AutoConnect=true for unmanaged wifi networks if
  // 'AllowOnlyPolicyNetworksToAutoconnect' policy is active.
  if (EnablesUnmanagedWifiAutoconnect(validated_user_settings.value()) &&
      AllowOnlyPolicyNetworksToAutoconnect()) {
    InvokeErrorCallback(service_path, std::move(error_callback),
                        kInvalidUserSettings);
    return;
  }

  // Fill in HexSSID field from contents of SSID field if not set already.
  chromeos::onc::FillInHexSSIDFieldsInOncObject(
      chromeos::onc::kNetworkConfigurationSignature,
      validated_user_settings.value());

  const base::Value::Dict* network_policy = policies->GetPolicyByGuid(guid);
  if (network_policy) {
    NET_LOG(DEBUG) << "Configuration is managed: " << NetworkId(state);
  }

  base::Value::Dict shill_dictionary = policy_util::CreateShillConfiguration(
      *profile, guid, policies->GetGlobalNetworkConfig(), network_policy,
      &validated_user_settings.value());

  SetShillProperties(service_path, std::move(shill_dictionary),
                     std::move(callback), std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::ClearShillProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  network_configuration_handler_->ClearShillProperties(
      service_path, names, std::move(callback), std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::SetManagedActiveProxyValues(
    const std::string& guid,
    base::Value::Dict* dictionary) {
  DCHECK(ui_proxy_config_service_);
  const std::string proxy_settings_key = ::onc::network_config::kProxySettings;

  base::Value::Dict* proxy_settings =
      dictionary->EnsureDict(proxy_settings_key);
  ui_proxy_config_service_->MergeEnforcedProxyConfig(guid, proxy_settings);

  if (proxy_settings->empty()) {
    dictionary->Remove(proxy_settings_key);
  }
}

void ManagedNetworkConfigurationHandlerImpl::SetShillProperties(
    const std::string& service_path,
    base::Value::Dict shill_dictionary,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  network_configuration_handler_->SetShillProperties(
      service_path, shill_dictionary, std::move(callback),
      std::move(error_callback));
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfiguration(
    const std::string& userhash,
    const base::Value::Dict& properties,
    network_handler::ServiceResultCallback callback,
    network_handler::ErrorCallback error_callback) const {
  std::string guid =
      GetStringFromDictionary(properties, ::onc::network_config::kGUID);
  const NetworkState* network_state = nullptr;
  if (!guid.empty()) {
    network_state = network_state_handler_->GetNetworkStateFromGuid(guid);
  }
  if (network_state) {
    NET_LOG(USER) << "CreateConfiguration for: " << NetworkId(network_state);
  } else {
    std::string type =
        GetStringFromDictionary(properties, ::onc::network_config::kType);
    NET_LOG(USER) << "Create new network configuration, Type: " << type;

    if (type == ::onc::network_type::kWiFi) {
      const base::Value::Dict* type_dict =
          properties.FindDict(::onc::network_config::kWiFi);
      const std::optional<bool> is_hidden =
          type_dict ? type_dict->FindBool(::onc::wifi::kHiddenSSID)
                    : std::nullopt;
      if (is_hidden.has_value()) {
        WifiNetworkMetricsHelper::LogInitiallyConfiguredAsHidden(*is_hidden);
      }
    }
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names. User settings are only partial ONC, thus we ignore missing fields.
  chromeos::onc::Validator validator(
      false,   // Ignore unknown fields.
      false,   // Ignore invalid recommended field names.
      false,   // Ignore missing fields.
      false,   // This ONC does not come from policy.
      false);  // Don't log warnings.

  chromeos::onc::Validator::Result validation_result;
  std::optional<base::Value::Dict> validated_properties =
      validator.ValidateAndRepairObject(
          &chromeos::onc::kNetworkConfigurationSignature, properties,
          &validation_result);
  if (validation_result == chromeos::onc::Validator::INVALID) {
    InvokeErrorCallback("", std::move(error_callback), kInvalidUserSettings);
    return;
  }
  if (validation_result == chromeos::onc::Validator::VALID_WITH_WARNINGS) {
    NET_LOG(DEBUG) << "Validation of ONC user settings produced warnings.";
  }

  // Fill in HexSSID field from contents of SSID field if not set already - this
  // is required to properly match the configuration against existing policies.
  chromeos::onc::FillInHexSSIDFieldsInOncObject(
      chromeos::onc::kNetworkConfigurationSignature,
      validated_properties.value());

  // Make sure the network is not configured through a user policy.
  const ProfilePolicies* policies = nullptr;
  if (!userhash.empty()) {
    policies = GetPoliciesForUser(userhash);
    if (!policies) {
      InvokeErrorCallback("", std::move(error_callback),
                          kPoliciesNotInitialized);
      return;
    }

    if (policies->HasPolicyMatchingShillProperties(
            validated_properties.value())) {
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

  if (policies->HasPolicyMatchingShillProperties(
          validated_properties.value())) {
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

  // If a GUID was provided, verify that the new configuration matches an
  // existing NetworkState for an unconfigured (i.e. visible) network.
  // Requires HexSSID to be set first for comparing SSIDs.
  if (!guid.empty()) {
    // |network_state| can by null if a network went out of range or was
    // forgotten while the UI is open. Configuration should succeed and the GUID
    // can be reused.
    if (network_state) {
      if (!MatchesExistingNetworkState(validated_properties.value(),
                                       network_state)) {
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
    guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  base::Value::Dict shill_dictionary =
      policy_util::CreateShillConfiguration(*profile, guid,
                                            nullptr,  // no global policy
                                            nullptr,  // no network policy
                                            &validated_properties.value());

  network_configuration_handler_->CreateShillConfiguration(
      shill_dictionary, std::move(callback), std::move(error_callback));
}

NetworkMetadataStore*
ManagedNetworkConfigurationHandlerImpl::GetNetworkMetadataStore() {
  if (network_metadata_store_for_testing_) {
    return network_metadata_store_for_testing_;
  }

  return NetworkHandler::Get()->network_metadata_store();
}

void ManagedNetworkConfigurationHandlerImpl::ConfigurePolicyNetwork(
    const base::Value::Dict& shill_properties,
    base::OnceClosure callback) const {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  network_configuration_handler_->CreateShillConfiguration(
      shill_properties,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork,
          weak_ptr_factory_.GetWeakPtr(), std::move(split_callback.first)),
      base::BindOnce(&LogErrorWithDictAndCallCallback,
                     std::move(split_callback.second), FROM_HERE));
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
    const base::Value::List& network_configs_onc,
    const base::Value::Dict& global_network_config) {
  VLOG(1) << "Setting policies from: " << ToDebugString(onc_source, userhash);

  // |userhash| must be empty for device policies.
  DCHECK(onc_source != ::onc::ONC_SOURCE_DEVICE_POLICY || userhash.empty());
  ProfilePolicies* policies = GetOrCreatePoliciesForUser(userhash);
  policies->SetGlobalNetworkConfig(global_network_config);

  // Update prohibited technologies if this is a device policy.
  if (onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY &&
      prohibited_technologies_handler_) {
    const base::Value::List* prohibited_list =
        policies->GetGlobalNetworkConfig()->FindList(
            ::onc::global_network_config::kDisableNetworkTypes);
    if (prohibited_list) {
      prohibited_technologies_handler_->SetProhibitedTechnologies(
          *prohibited_list);
    } else {
      // An empty list is provided to guarantee that all technologies are
      // explicitly allowed if the policy being applied is a device policy that
      // does not specifically prohibit any technologies.
      prohibited_technologies_handler_->SetProhibitedTechnologies(
          base::Value::List());
    }
  }

  ApplyOrQueuePolicies(
      userhash, policies->ApplyOncNetworkConfigurationList(network_configs_onc),
      /*can_affect_other_networks=*/true, /*options=*/{});

  ApplyDisconnectWiFiOnEthernetPolicy();

  for (auto& observer : observers_)
    observer.PoliciesChanged(userhash);
}

void ManagedNetworkConfigurationHandlerImpl::
    ApplyDisconnectWiFiOnEthernetPolicy() {
  const std::string* disconnect_wifi_policy = FindGlobalPolicyString(
      ::onc::global_network_config::kDisconnectWiFiOnEthernet);
  if (disconnect_wifi_policy) {
    base::Value shill_property_value =
        base::Value(shill::kDisconnectWiFiOnEthernetOff);
    if (*disconnect_wifi_policy ==
        ::onc::global_network_config::kDisconnectWiFiOnEthernetWhenConnected) {
      shill_property_value =
          base::Value(shill::kDisconnectWiFiOnEthernetConnected);
    }
    if (*disconnect_wifi_policy ==
        ::onc::global_network_config::kDisconnectWiFiOnEthernetWhenOnline) {
      shill_property_value =
          base::Value(shill::kDisconnectWiFiOnEthernetOnline);
    }
    network_configuration_handler_->SetManagerProperty(
        shill::kDisconnectWiFiOnEthernetProperty, shill_property_value);
  }
}

bool ManagedNetworkConfigurationHandlerImpl::IsAnyPolicyApplicationRunning()
    const {
  for (const auto& [_, policy_application_info] :
       policy_application_info_map_) {
    if (policy_application_info.IsRunningOrRequired()) {
      return true;
    }
  }
  return false;
}

void ManagedNetworkConfigurationHandlerImpl::ApplyOrQueuePolicies(
    const std::string& userhash,
    base::flat_set<std::string> modified_policies,
    bool can_affect_other_networks,
    PolicyApplicator::Options options) {
  // Note that this will default-construct a PolicyApplicationInfo if none
  // exists for the shill profile identifier by |userhash| yet.
  PolicyApplicationInfo& policy_application_info =
      policy_application_info_map_[userhash];
  policy_application_info.options.Merge(options);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    VLOG(1) << "The relevant Shill profile isn't initialized yet, postponing "
            << "policy application.";
    // OnProfileAdded will apply all policies for this userhash.
    return;
  }
  if (!can_affect_other_networks && modified_policies.empty()) {
    // The change can not affect managed networks (e.g. because it only
    // affected resolved client certificates) but no policy
    // NetworkConfigurations have changed, so there can be no effective change.
    // Avoid scheduling a policy application for this.
    // This can happen e.g. if a ONC variable changes which is not used by any
    // NetworkConfiguration.
    return;
  }

  policy_application_info.modified_policy_guids.insert(
      std::make_move_iterator(modified_policies.begin()),
      std::make_move_iterator(modified_policies.end()));
  SchedulePolicyApplication(userhash);
}

void ManagedNetworkConfigurationHandlerImpl::SchedulePolicyApplication(
    const std::string& userhash) {
  PolicyApplicationInfo& policy_application_info =
      policy_application_info_map_[userhash];
  policy_application_info.application_required = true;
  if (policy_application_info.task_scheduled) {
    return;
  }
  policy_application_info.task_scheduled = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::StartPolicyApplication,
          weak_ptr_factory_.GetWeakPtr(), userhash));
}

void ManagedNetworkConfigurationHandlerImpl::StartPolicyApplication(
    const std::string& userhash) {
  PolicyApplicationInfo& policy_application_info =
      policy_application_info_map_[userhash];
  DCHECK(policy_application_info.task_scheduled);
  DCHECK(policy_application_info.application_required);
  policy_application_info.task_scheduled = false;
  policy_application_info.application_required = false;

  const ProfilePolicies* policies = policies_by_user_[userhash].get();
  DCHECK(policies);

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForUserhash(userhash);
  if (!profile) {
    // The shill profile has been removed in the meantime. This could happen
    // e.g. if the user session is exiting or the device is shutting down.
    // See b/240237232 for context.
    return;
  }

  base::flat_set<std::string> modified_guids;
  policy_application_info.modified_policy_guids.swap(modified_guids);

  PolicyApplicator::Options options =
      std::move(policy_application_info.options);
  policy_application_info.options = {};

  policy_application_info.running_policy_applicator =
      std::make_unique<PolicyApplicator>(
          *profile, policies->GetGuidToPolicyMap(),
          policies->GetGlobalNetworkConfig()->Clone(), this,
          managed_cellular_pref_handler_, std::move(modified_guids),
          std::move(options));
  policy_application_info.running_policy_applicator->Run();
}

void ManagedNetworkConfigurationHandlerImpl::SetProfileWideVariableExpansions(
    const std::string& userhash,
    base::flat_map<std::string, std::string> expansions) {
  ApplyOrQueuePolicies(
      userhash,
      GetOrCreatePoliciesForUser(userhash)->SetProfileWideExpansions(
          std::move(expansions)),
      /*can_affect_other_networks=*/false,
      /*options=*/{});
}

bool ManagedNetworkConfigurationHandlerImpl::SetResolvedClientCertificate(
    const std::string& userhash,
    const std::string& guid,
    client_cert::ResolvedCert resolved_cert) {
  bool change_had_effect =
      GetOrCreatePoliciesForUser(userhash)->SetResolvedClientCertificate(
          guid, std::move(resolved_cert));
  if (!change_had_effect)
    return false;
  ApplyOrQueuePolicies(userhash, {guid},
                       /*can_affect_other_networks=*/false, /*options=*/{});
  return true;
}

void ManagedNetworkConfigurationHandlerImpl::set_ui_proxy_config_service(
    UIProxyConfigService* ui_proxy_config_service) {
  ui_proxy_config_service_ = ui_proxy_config_service;
}

void ManagedNetworkConfigurationHandlerImpl::set_user_prefs(
    PrefService* user_prefs) {
  user_prefs_ = user_prefs;
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileAdded(
    const NetworkProfile& profile) {
  VLOG(1) << "Adding profile: " << profile.ToDebugString();

  const ProfilePolicies* policies = GetPoliciesForProfile(profile);
  if (!policies) {
    VLOG(1) << "The relevant policy is not initialized, "
            << "postponing policy application.";
    // See SetPolicy.
    return;
  }

  // The profile's network policy may have a GlobalNetworkConfiguration which
  // can affect unmanaged networks (see ApplyGlobalPolicyOnUnmanagedEntry in
  // PolicyApplicator), so set can_affect_other_networks to true.
  ApplyOrQueuePolicies(profile.userhash, policies->GetAllPolicyGuids(),
                       /*can_affect_other_networks=*/true, /*options=*/{});
}

void ManagedNetworkConfigurationHandlerImpl::OnProfileRemoved(
    const NetworkProfile& profile) {
  // Nothing to do in this case.
}

void ManagedNetworkConfigurationHandlerImpl::CreateConfigurationFromPolicy(
    const base::Value::Dict& shill_properties,
    base::OnceClosure callback) {
  ConfigurePolicyNetwork(shill_properties, std::move(callback));
}

void ManagedNetworkConfigurationHandlerImpl::
    UpdateExistingConfigurationWithPropertiesFromPolicy(
        const base::Value::Dict& existing_properties,
        const base::Value::Dict& new_properties,
        base::OnceClosure callback) {
  base::Value::Dict shill_properties;

  const std::string* profile =
      existing_properties.FindString(shill::kProfileProperty);
  if (!profile || profile->empty()) {
    // TODO(b/258782165): Figure out how to deal with entries that don't have a
    // Profile property properly.
    NET_LOG(ERROR) << "Missing profile property: "
                   << shill_property_util::GetNetworkIdFromProperties(
                          existing_properties);
    std::move(callback).Run();
    return;
  }
  shill_properties.Set(shill::kProfileProperty, *profile);

  if (!shill_property_util::CopyIdentifyingProperties(
          existing_properties,
          /*properties_read_from_shill=*/true, &shill_properties)) {
    NET_LOG(ERROR) << "Missing identifying properties",
        shill_property_util::GetNetworkIdFromProperties(existing_properties);
  }

  shill_properties.Merge(new_properties.Clone());

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  network_configuration_handler_->CreateShillConfiguration(
      shill_properties,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork,
          weak_ptr_factory_.GetWeakPtr(), std::move(split_callback.first)),
      base::BindOnce(&LogErrorWithDictAndCallCallback,
                     std::move(split_callback.second), FROM_HERE));
}

void ManagedNetworkConfigurationHandlerImpl::TriggerCellularPolicyApplication(
    const NetworkProfile& profile,
    const base::flat_set<std::string>& new_cellular_policy_guids) {
  const ProfilePolicies* policies = GetPoliciesForUser(profile.userhash);
  DCHECK(policies);

  if (!cellular_policy_handler_) {
    NET_LOG(ERROR) << "Unable to attempt policy eSIM installation since "
                   << "CellularPolicyHandler has not been initialized";
    return;
  }

  for (const std::string& guid : new_cellular_policy_guids) {
    const base::Value::Dict* network_policy = policies->GetPolicyByGuid(guid);
    DCHECK(network_policy);

    cellular_policy_handler_->InstallESim(*network_policy);
  }
}

void ManagedNetworkConfigurationHandlerImpl::OnCellularPoliciesApplied(
    const NetworkProfile& profile) {
  const std::string& userhash = profile.userhash;
  bool is_device_policy = userhash.empty();

  // Inform observers that cellular policy application has finished.
  // Only do this if non-cellular policy application is already done for
  // `profile`, otherwise wait for OnPoliciesApplied to send the notification.
  // Note that currently there is no separate observer signal for this.
  if ((is_device_policy && device_policy_applied_) ||
      (!is_device_policy && user_policy_applied_)) {
    for (auto& observer : observers_)
      observer.PoliciesApplied(userhash);
  }
}

void ManagedNetworkConfigurationHandlerImpl::
    OnEnterpriseMonitoredWebPoliciesApplied() const {
  for (auto& observer : observers_) {
    observer.PoliciesApplied(std::string());
  }
}

void ManagedNetworkConfigurationHandlerImpl::OnPoliciesApplied(
    const NetworkProfile& profile,
    const base::flat_set<std::string>& new_cellular_policy_guids) {
  const std::string& userhash = profile.userhash;
  VLOG(1) << "Policy application for user '" << userhash << "' finished.";

  PolicyApplicationInfo& policy_application_info =
      policy_application_info_map_[userhash];

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(policy_application_info.running_policy_applicator));

  TriggerCellularPolicyApplication(profile, new_cellular_policy_guids);

  if (policy_application_info.application_required) {
    // This means that a network policy change happened while policy was being
    // applied. Schedule another network policy application run.
    SchedulePolicyApplication(userhash);
    return;
  }

  if (userhash.empty())
    device_policy_applied_ = true;
  else
    user_policy_applied_ = true;

  ESimPolicyLoginMetricsLogger::RecordBlockNonManagedCellularBehavior(
      AllowOnlyPolicyCellularNetworks());
  // Call UpdateBlockCellularNetworks when either device policy applied or
  // user policy applied so that so that unmanaged cellular networks are
  // blocked correctly if the policy appears in either.
  network_state_handler_->UpdateBlockedCellularNetworks(
      AllowOnlyPolicyCellularNetworks());

  if (network_device_handler_) {
    network_device_handler_->SetAllowCellularSimLock(AllowCellularSimLock());
  }

  if (features::IsApnRevampAndAllowApnModificationPolicyEnabled()) {
    ModifyCustomAPNs();
  }

  if (hotspot_controller_) {
    hotspot_controller_->SetPolicyAllowHotspot(AllowCellularHotspot());
  }

  if (device_policy_applied_ && user_policy_applied_) {
    network_state_handler_->UpdateBlockedWifiNetworks(
        AllowOnlyPolicyWiFiToConnect(),
        AllowOnlyPolicyWiFiToConnectIfAvailable(), GetBlockedHexSSIDs());
  }

  for (auto& observer : observers_)
    observer.PoliciesApplied(userhash);
}

void ManagedNetworkConfigurationHandlerImpl::ModifyCustomAPNs() {
  NetworkStateHandler::NetworkStateList networks;

  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Cellular(),
                                               /*configured_only=*/false,
                                               /*visible_only=*/false,
                                               /*limit=*/0, &networks);

  base::UmaHistogramBoolean(
      "Network.Ash.Cellular.Apn.Policy.AllowApnModification",
      AllowApnModification());

  for (const NetworkState* network : networks) {
    if (network->IsManagedByPolicy()) {
      continue;
    }
    const base::Value::List* existing_custom_apn_list =
        GetNetworkMetadataStore()->GetCustomApnList(network->guid());
    if (existing_custom_apn_list) {
      base::Value::Dict onc;
      onc.Set(::onc::network_config::kGUID, network->guid());
      onc.Set(::onc::network_config::kType, ::onc::network_type::kCellular);
      base::Value::Dict type_dict;
      type_dict.Set(::onc::cellular::kCustomAPNList,
                    AllowApnModification() ? existing_custom_apn_list->Clone()
                                           : base::Value::List());
      onc.Set(::onc::network_type::kCellular, std::move(type_dict));
      SetProperties(network->path(), onc, base::DoNothing(),
                    base::BindOnce(&OnSetCustomApnFailure));
    }
  }
}

const base::Value::Dict*
ManagedNetworkConfigurationHandlerImpl::FindPolicyByGUID(
    const std::string userhash,
    const std::string& guid,
    ::onc::ONCSource* onc_source) const {
  *onc_source = ::onc::ONC_SOURCE_NONE;

  if (!userhash.empty()) {
    const ProfilePolicies* user_policies = GetPoliciesForUser(userhash);
    if (user_policies) {
      const base::Value::Dict* policy = user_policies->GetPolicyByGuid(guid);
      if (policy) {
        *onc_source = ::onc::ONC_SOURCE_USER_POLICY;
        return policy;
      }
    }
  }

  const ProfilePolicies* device_policies =
      GetPoliciesForUser(/*userhash=*/std::string());
  if (device_policies) {
    const base::Value::Dict* policy = device_policies->GetPolicyByGuid(guid);
    if (policy) {
      *onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
      return policy;
    }
  }

  return nullptr;
}

void ManagedNetworkConfigurationHandlerImpl::ResetDNSPropertiesCallback(
    const std::string& service_path,
    std::optional<base::Value::Dict> network_properties,
    std::optional<std::string> error) {
  if (!network_properties) {
    return;
  }

  // Create a dictionary of the relevant properties to set.
  base::Value::Dict reset_dns_properties;

  // NameServersConfigType - to be deterined by DHCP.
  reset_dns_properties.Set(::onc::network_config::kNameServersConfigType,
                           ::onc::network_config::kIPConfigTypeDHCP);

  // IPAddressConfigType - set to whatever previously existed.
  base::Value* ip_address_config_type =
      network_properties->Find(::onc::network_config::kIPAddressConfigType);
  reset_dns_properties.Set(::onc::network_config::kIPAddressConfigType,
                           std::move(*ip_address_config_type));

  // StaticIPConfig - set to what previously existed if either
  // kNameServersConfigType or kIPAddressConfigType was previously set to
  // "Static" - the NameServers field is cleared later through the ONC
  // Validator in SetProperties.
  base::Value* static_ip_config =
      network_properties->Find(::onc::network_config::kStaticIPConfig);
  if (static_ip_config) {
    reset_dns_properties.Set(::onc::network_config::kStaticIPConfig,
                             std::move(*static_ip_config));
  }

  // Type - set to the existing network type, (wifi, ethernet, etc).
  base::Value* type = network_properties->Find(::onc::network_config::kType);
  reset_dns_properties.Set(::onc::network_config::kType, std::move(*type));

  // The policy is then translated and applied to the shill service.
  SetProperties(service_path, std::move(reset_dns_properties),
                base::DoNothing(),
                base::BindOnce(&OnResetDnsPropertiesFailure));
}

void ManagedNetworkConfigurationHandlerImpl::ResetDNSProperties(
    const std::string& service_path) {
  GetProperties(
      /*userhash*/ std::string(), service_path,
      base::BindOnce(
          &ManagedNetworkConfigurationHandlerImpl::ResetDNSPropertiesCallback,
          weak_ptr_factory_.GetWeakPtr()));
}

bool ManagedNetworkConfigurationHandlerImpl::HasAnyPolicyNetwork(
    const std::string& userhash) const {
  const ProfilePolicies* policies = GetPoliciesForUser(userhash);
  if (!policies)
    return false;

  return !policies->GetAllPolicyGuids().empty();
}

const base::Value::Dict*
ManagedNetworkConfigurationHandlerImpl::GetGlobalConfigFromPolicy(
    const std::string& userhash) const {
  const ProfilePolicies* policies = GetPoliciesForUser(userhash);
  if (!policies)
    return nullptr;

  return policies->GetGlobalNetworkConfig();
}

const base::Value::Dict*
ManagedNetworkConfigurationHandlerImpl::FindPolicyByGuidAndProfile(
    const std::string& guid,
    const std::string& profile_path,
    PolicyType policy_type,
    ::onc::ONCSource* out_onc_source,
    std::string* out_userhash) const {
  if (profile_path.empty())
    return nullptr;

  const NetworkProfile* profile =
      network_profile_handler_->GetProfileForPath(profile_path);
  if (!profile) {
    NET_LOG(ERROR) << "Profile path unknown:" << profile_path
                   << " For: " << NetworkGuidId(guid);
    return nullptr;
  }

  const ProfilePolicies* policies = GetPoliciesForProfile(*profile);
  if (!policies)
    return nullptr;

  const base::Value::Dict* policy =
      (policy_type == PolicyType::kOriginal)
          ? policies->GetOriginalPolicyByGuid(guid)
          : policies->GetPolicyByGuid(guid);
  if (policy && out_onc_source) {
    *out_onc_source =
        (profile->userhash.empty() ? ::onc::ONC_SOURCE_DEVICE_POLICY
                                   : ::onc::ONC_SOURCE_USER_POLICY);
  }
  if (policy && out_userhash) {
    *out_userhash = profile->userhash;
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

PolicyTextMessageSuppressionState
ManagedNetworkConfigurationHandlerImpl::GetAllowTextMessages() const {
  const std::string* allow_text_messages =
      FindGlobalPolicyString(::onc::global_network_config::kAllowTextMessages);
  if (!allow_text_messages) {
    return PolicyTextMessageSuppressionState::kUnset;
  }

  if (*allow_text_messages == ::onc::cellular::kTextMessagesAllow) {
    return PolicyTextMessageSuppressionState::kAllow;
  }

  if (*allow_text_messages == ::onc::cellular::kTextMessagesSuppress) {
    return PolicyTextMessageSuppressionState::kSuppress;
  }

  return PolicyTextMessageSuppressionState::kUnset;
}

bool ManagedNetworkConfigurationHandlerImpl::AllowApnModification() const {
  return FindGlobalPolicyBool(
             ::onc::global_network_config::kAllowAPNModification)
      .value_or(true);
}

bool ManagedNetworkConfigurationHandlerImpl::AllowCellularSimLock() const {
  return FindGlobalPolicyBool(
             ::onc::global_network_config::kAllowCellularSimLock)
      .value_or(true);
}

bool ManagedNetworkConfigurationHandlerImpl::AllowCellularHotspot() const {
  return FindGlobalPolicyBool(
             ::onc::global_network_config::kAllowCellularHotspot)
      .value_or(true);
}

bool ManagedNetworkConfigurationHandlerImpl::AllowOnlyPolicyCellularNetworks()
    const {
  return FindGlobalPolicyBool(
             ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks)
      .value_or(false);
}

bool ManagedNetworkConfigurationHandlerImpl::AllowOnlyPolicyWiFiToConnect()
    const {
  return FindGlobalPolicyBool(
             ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect)
      .value_or(false);
}

bool ManagedNetworkConfigurationHandlerImpl::
    AllowOnlyPolicyWiFiToConnectIfAvailable() const {
  return FindGlobalPolicyBool(::onc::global_network_config::
                                  kAllowOnlyPolicyWiFiToConnectIfAvailable)
      .value_or(false);
}

bool ManagedNetworkConfigurationHandlerImpl::
    AllowOnlyPolicyNetworksToAutoconnect() const {
  return FindGlobalPolicyBool(::onc::global_network_config::
                                  kAllowOnlyPolicyNetworksToAutoconnect)
      .value_or(false);
}

bool ManagedNetworkConfigurationHandlerImpl::RecommendedValuesAreEphemeral()
    const {
  DCHECK(policy_util::AreEphemeralNetworkPoliciesEnabled());
  return FindGlobalPolicyBool(
             ::onc::global_network_config::kRecommendedValuesAreEphemeral)
      .value_or(false);
}

bool ManagedNetworkConfigurationHandlerImpl::
    UserCreatedNetworkConfigurationsAreEphemeral() const {
  DCHECK(policy_util::AreEphemeralNetworkPoliciesEnabled());
  return FindGlobalPolicyBool(::onc::global_network_config::
                                  kUserCreatedNetworkConfigurationsAreEphemeral)
      .value_or(false);
}

bool ManagedNetworkConfigurationHandlerImpl::IsProhibitedFromConfiguringVpn()
    const {
  if (!user_prefs_ ||
      !user_prefs_->FindPreference(arc::prefs::kAlwaysOnVpnPackage) ||
      !user_prefs_->FindPreference(prefs::kVpnConfigAllowed)) {
    return false;
  }

  // When an admin Activate Always ON VPN for all user traffic with an Android
  // VPN, arc::prefs::kAlwaysOnVpnPackage will be non empty. If additionally,
  // the admin prohibits users from disconnecting from a VPN manually,
  // prefs::kVpnConfigAllowed becomes false. See go/test-cros-vpn-policies.
  return !user_prefs_->GetString(arc::prefs::kAlwaysOnVpnPackage).empty() &&
         !user_prefs_->GetBoolean(prefs::kVpnConfigAllowed);
}

std::vector<std::string>
ManagedNetworkConfigurationHandlerImpl::GetBlockedHexSSIDs() const {
  const base::Value::List* blocked_value =
      FindGlobalPolicyList(::onc::global_network_config::kBlockedHexSSIDs);
  if (!blocked_value) {
    return std::vector<std::string>();
  }

  std::vector<std::string> blocked_hex_ssids;
  for (const base::Value& entry : *blocked_value) {
    blocked_hex_ssids.push_back(entry.GetString());
  }
  return blocked_hex_ssids;
}

ProfilePolicies*
ManagedNetworkConfigurationHandlerImpl::GetOrCreatePoliciesForUser(
    const std::string& userhash) {
  auto it = policies_by_user_.find(userhash);

  ProfilePolicies* policies = nullptr;
  if (it != policies_by_user_.end()) {
    policies = it->second.get();
  } else {
    auto policies_owned = std::make_unique<ProfilePolicies>();
    policies = policies_owned.get();
    policies_by_user_[userhash] = std::move(policies_owned);
  }
  return policies;
}

const ProfilePolicies*
ManagedNetworkConfigurationHandlerImpl::GetPoliciesForUser(
    const std::string& userhash) const {
  auto it = policies_by_user_.find(userhash);
  return it != policies_by_user_.end() ? it->second.get() : nullptr;
}

const ProfilePolicies*
ManagedNetworkConfigurationHandlerImpl::GetPoliciesForProfile(
    const NetworkProfile& profile) const {
  DCHECK(profile.type() != NetworkProfile::TYPE_SHARED ||
         profile.userhash.empty());
  return GetPoliciesForUser(profile.userhash);
}

ManagedNetworkConfigurationHandlerImpl::
    ManagedNetworkConfigurationHandlerImpl() {
  CHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
}

ManagedNetworkConfigurationHandlerImpl::
    ~ManagedNetworkConfigurationHandlerImpl() {
  if (network_profile_handler_ && network_profile_handler_->HasObserver(this))
    network_profile_handler_->RemoveObserver(this);

  Shutdown();
}

void ManagedNetworkConfigurationHandlerImpl::Init(
    CellularPolicyHandler* cellular_policy_handler,
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    NetworkStateHandler* network_state_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkDeviceHandler* network_device_handler,
    ProhibitedTechnologiesHandler* prohibited_technologies_handler,
    HotspotController* hotspot_controller) {
  cellular_policy_handler_ = cellular_policy_handler;
  managed_cellular_pref_handler_ = managed_cellular_pref_handler;
  network_state_handler_ = network_state_handler;
  network_profile_handler_ = network_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_device_handler_ = network_device_handler;
  if (network_profile_handler_)
    network_profile_handler_->AddObserver(this);
  prohibited_technologies_handler_ = prohibited_technologies_handler;
  hotspot_controller_ = hotspot_controller;
}

void ManagedNetworkConfigurationHandlerImpl::OnPolicyAppliedToNetwork(
    base::OnceClosure callback,
    const std::string& service_path,
    const std::string& guid) const {
  // When this is called, the policy has been fully applied and is reflected in
  // NetworkStateHandler, so it is safe to notify observers.
  // Notifying observers is the last step of policy application to
  // |service_path|.
  NotifyPolicyAppliedToNetwork(service_path);

  // Inform the caller that has requested policy application that it has
  // finished.
  std::move(callback).Run();
}

// Get{Managed}Properties helpers

void ManagedNetworkConfigurationHandlerImpl::GetDeviceStateProperties(
    const std::string& service_path,
    base::Value::Dict* properties) {
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
    properties->Set(shill::kAddressProperty, device_state->mac_address());
  }

  // Get the IPConfig properties from the device and store them in "IPConfigs"
  // (plural) in the properties dictionary. (Note: Shill only provides a single
  // "IPConfig" property for a network service, but a consumer of this API may
  // want information about all ipv4 and ipv6 IPConfig properties.
  base::Value::List ip_configs;

  if (!device_state || device_state->ip_configs().empty()) {
    // Shill may not provide IPConfigs for external Cellular devices/dongles
    // (https://crbug.com/739314) or VPNs, so build a dictionary of ipv4
    // properties from cached NetworkState properties .
    NET_LOG(DEBUG)
        << "GetDeviceStateProperties: Setting IPv4 properties from network: "
        << NetworkId(network);
    if (network->ipv4_config().has_value()) {
      ip_configs.Append(network->ipv4_config()->Clone());
    }
  } else {
    // Convert the DeviceState IPConfigs dictionary to a list.
    for (const auto iter : device_state->ip_configs()) {
      ip_configs.Append(iter.second.Clone());
    }
  }
  if (!ip_configs.empty()) {
    properties->Set(shill::kIPConfigsProperty, std::move(ip_configs));
  }
}

void ManagedNetworkConfigurationHandlerImpl::GetPropertiesCallback(
    PropertiesType properties_type,
    const std::string& userhash,
    network_handler::PropertiesCallback callback,
    const std::string& service_path,
    std::optional<base::Value::Dict> shill_properties) {
  if (!shill_properties) {
    SendProperties(properties_type, userhash, service_path, std::move(callback),
                   std::nullopt);
    return;
  }

  const std::string* guid = shill_properties->FindString(shill::kGuidProperty);
  if (!guid || guid->empty()) {
    // Unmanaged networks are assigned a GUID in NetworkState. Provide this
    // value in the ONC dictionary.
    const NetworkState* state =
        network_state_handler_->GetNetworkState(service_path);
    if (state && !state->guid().empty()) {
      shill_properties->Set(shill::kGuidProperty, state->guid());
    } else {
      NET_LOG(ERROR) << "Network has no GUID specified: "
                     << NetworkPathId(service_path);
    }
  }

  const std::string* type = shill_properties->FindString(shill::kTypeProperty);
  // Add any associated DeviceState properties.
  GetDeviceStateProperties(service_path, &shill_properties.value());

  // Only request additional Device properties for Cellular networks with a
  // valid device.
  if (network_device_handler_ && *type == shill::kTypeCellular) {
    std::string* device_path =
        shill_properties->FindString(shill::kDeviceProperty);
    if (device_path && !device_path->empty() &&
        *device_path != kEmptyServicePath) {
      // Request the device properties. On success or failure pass (a possibly
      // modified) |shill_properties| to |send_callback|.
      network_device_handler_->GetDeviceProperties(
          *device_path,
          base::BindOnce(
              &ManagedNetworkConfigurationHandlerImpl::OnGetDeviceProperties,
              weak_ptr_factory_.GetWeakPtr(), properties_type, userhash,
              service_path, std::move(callback), std::move(shill_properties)));
      return;
    }
  }

  SendProperties(properties_type, userhash, service_path, std::move(callback),
                 std::move(shill_properties));
}

void ManagedNetworkConfigurationHandlerImpl::OnGetDeviceProperties(
    PropertiesType properties_type,
    const std::string& userhash,
    const std::string& service_path,
    network_handler::PropertiesCallback callback,
    std::optional<base::Value::Dict> network_properties,
    const std::string& device_path,
    std::optional<base::Value::Dict> device_properties) {
  DCHECK(network_properties);
  if (!device_properties) {
    NET_LOG(ERROR) << "Error getting device properties: "
                   << NetworkPathId(service_path);
  } else {
    // Create a "Device" dictionary in |network_properties|.
    network_properties->Set(shill::kDeviceProperty,
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
    std::optional<base::Value::Dict> shill_properties) {
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
    std::move(callback).Run(service_path, std::nullopt,
                            network_handler::kDBusFailedError);
    return;
  }
  const std::string* guid = shill_properties->FindString(shill::kGuidProperty);
  if (!guid) {
    NET_LOG(ERROR) << get_name(properties_type) << " Missing GUID.";
    std::move(callback).Run(service_path, std::nullopt, kUnknownNetwork);
    return;
  }

  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  ::onc::ONCSource onc_source;
  FindPolicyByGUID(userhash, *guid, &onc_source);
  base::Value::Dict onc_network = onc::TranslateShillServiceToONCPart(
      shill_properties.value(), onc_source,
      &chromeos::onc::kNetworkWithStateSignature, network_state);

  if (properties_type == PropertiesType::kUnmanaged) {
    std::move(callback).Run(
        service_path, std::make_optional(std::move(onc_network)), std::nullopt);
    return;
  }

  const std::string* profile_path =
      shill_properties->FindString(shill::kProfileProperty);
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
      shill_property_util::GetUIDataFromProperties(shill_properties.value());

  const base::Value::Dict* user_settings = nullptr;

  if (ui_data && profile) {
    user_settings = ui_data->GetUserSettingsDictionary();
  } else if (profile) {
    NET_LOG(DEBUG) << "Network contains empty or invalid UIData: "
                   << NetworkId(network_state);
    // TODO(pneubeck): add a conversion of user configured entries of old
    // ChromeOS versions. We will have to use a heuristic to determine which
    // properties _might_ be user configured.
  }

  const base::Value::Dict* network_policy = nullptr;
  const base::Value::Dict* global_policy = nullptr;
  if (profile) {
    const ProfilePolicies* policies = GetPoliciesForProfile(*profile);
    if (!policies) {
      NET_LOG(ERROR) << "GetManagedProperties failed: "
                     << kPoliciesNotInitialized;
      std::move(callback).Run(service_path, std::nullopt,
                              kPoliciesNotInitialized);
      return;
    }
    if (!guid->empty()) {
      network_policy = policies->GetPolicyByGuid(*guid);
    }
    global_policy = policies->GetGlobalNetworkConfig();
  }

  base::Value::Dict augmented_properties = policy_util::CreateManagedONC(
      global_policy, network_policy, user_settings, &onc_network, profile);
  SetManagedActiveProxyValues(*guid, &augmented_properties);
  std::move(callback).Run(service_path,
                          std::make_optional(std::move(augmented_properties)),
                          std::nullopt);
}

void ManagedNetworkConfigurationHandlerImpl::NotifyPolicyAppliedToNetwork(
    const std::string& service_path) const {
  DCHECK(!service_path.empty());

  for (auto& observer : observers_)
    observer.PolicyAppliedToNetwork(service_path);
}

std::optional<bool>
ManagedNetworkConfigurationHandlerImpl::FindGlobalPolicyBool(
    std::string_view key) const {
  const base::Value::Dict* global_network_config = GetGlobalConfigFromPolicy(
      std::string() /* no username hash, device policy */);

  if (!global_network_config) {
    return {};
  }

  return global_network_config->FindBool(key);
}

const base::Value::List*
ManagedNetworkConfigurationHandlerImpl::FindGlobalPolicyList(
    std::string_view key) const {
  const base::Value::Dict* global_network_config = GetGlobalConfigFromPolicy(
      std::string() /* no username hash, device policy */);

  if (!global_network_config) {
    return nullptr;
  }

  return global_network_config->FindList(key);
}

const std::string*
ManagedNetworkConfigurationHandlerImpl::FindGlobalPolicyString(
    std::string_view key) const {
  const base::Value::Dict* global_network_config = GetGlobalConfigFromPolicy(
      std::string() /* no username hash, device policy */);

  if (!global_network_config) {
    return nullptr;
  }

  return global_network_config->FindString(key);
}

void ManagedNetworkConfigurationHandlerImpl::
    TriggerEphemeralNetworkConfigActions() {
  DCHECK(policy_util::AreEphemeralNetworkPoliciesEnabled());

  PolicyApplicator::Options options;
  options.reset_recommended_managed_configs = RecommendedValuesAreEphemeral();
  options.remove_unmanaged_configs =
      UserCreatedNetworkConfigurationsAreEphemeral();
  ApplyOrQueuePolicies(
      /*userhash=*/std::string(), /*modified_policies=*/{},
      /*can_affect_other_networks=*/true, std::move(options));
}

}  // namespace ash
