// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/policy_applicator.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

void LogErrorMessageAndInvokeCallback(base::OnceClosure callback,
                                      const base::Location& from_where,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  LOG(ERROR) << from_where.ToString() << ": " << error_message;
  std::move(callback).Run();
}

const base::Value::Dict* GetByGUID(
    const base::flat_map<std::string, base::Value::Dict>& policies,
    const std::string& guid) {
  auto it = policies.find(guid);
  if (it == policies.end())
    return nullptr;
  return &(it->second);
}

const base::Value::Dict* FindMatchingPolicy(
    const base::flat_map<std::string, base::Value::Dict>& policies,
    const base::Value::Dict& actual_network) {
  for (auto& policy : policies) {
    if (policy_util::IsPolicyMatching(policy.second, actual_network))
      return &(policy.second);
  }
  return nullptr;
}

// Returns the GUID property from |onc_part|, or an empty string if no GUID was
// present.
std::string GetGUIDFromONCPart(const base::Value::Dict& onc_part) {
  const std::string* guid_value =
      onc_part.FindString(::onc::network_config::kGUID);

  return guid_value ? *guid_value : std::string();
}

void CopyStringKey(const base::Value::Dict& old_shill_properties,
                   base::Value::Dict* new_shill_properties,
                   const std::string& property_key_name) {
  const std::string* value_in_old_entry =
      old_shill_properties.FindString(property_key_name);
  const std::string* value_in_new_entry =
      new_shill_properties->FindString(property_key_name);
  if (value_in_old_entry &&
      (!value_in_new_entry || value_in_new_entry->empty())) {
    NET_LOG(EVENT) << "Copying " << property_key_name
                   << " over to the new Shill entry, value: "
                   << *value_in_old_entry;
    new_shill_properties->Set(property_key_name, *value_in_old_entry);
  }
}

void CopyRequiredCellularProperties(
    const base::Value::Dict& old_shill_properties,
    base::Value::Dict* new_shill_properties) {
  const std::string* type =
      old_shill_properties.FindString(shill::kTypeProperty);
  if (!type || *type != shill::kTypeCellular) {
    return;
  }

  CopyStringKey(old_shill_properties, new_shill_properties,
                shill::kIccidProperty);
  CopyStringKey(old_shill_properties, new_shill_properties,
                shill::kEidProperty);
}

}  // namespace

PolicyApplicator::Options::Options() = default;

PolicyApplicator::Options::Options(PolicyApplicator::Options&& other) {
  reset_recommended_managed_configs = other.reset_recommended_managed_configs;
  remove_unmanaged_configs = other.remove_unmanaged_configs;
  // Reset the moved-from object to its default-constructed state.
  other.reset_recommended_managed_configs = false;
  other.remove_unmanaged_configs = false;
}

PolicyApplicator::Options& PolicyApplicator::Options::operator=(
    PolicyApplicator::Options&& other) {
  reset_recommended_managed_configs = other.reset_recommended_managed_configs;
  remove_unmanaged_configs = other.remove_unmanaged_configs;
  // Reset the moved-from object to its default-constructed state.
  other.reset_recommended_managed_configs = false;
  other.remove_unmanaged_configs = false;

  return *this;
}

PolicyApplicator::Options::~Options() = default;

void PolicyApplicator::Options::Merge(const PolicyApplicator::Options& other) {
  reset_recommended_managed_configs |= other.reset_recommended_managed_configs;
  remove_unmanaged_configs |= other.remove_unmanaged_configs;
}

PolicyApplicator::PolicyApplicator(
    const NetworkProfile& profile,
    base::flat_map<std::string, base::Value::Dict> all_policies,
    base::Value::Dict global_network_config,
    ConfigurationHandler* handler,
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    base::flat_set<std::string> modified_policy_guids,
    Options options)
    : handler_(handler),
      managed_cellular_pref_handler_(managed_cellular_pref_handler),
      profile_(profile),
      all_policies_(std::move(all_policies)),
      global_network_config_(std::move(global_network_config)),
      options_(std::move(options)),
      remaining_policy_guids_(std::move(modified_policy_guids)) {}

PolicyApplicator::~PolicyApplicator() {
  VLOG(1) << "Destroying PolicyApplicator for " << profile_.userhash;
}

void PolicyApplicator::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShillProfileClient::Get()->GetProperties(
      dbus::ObjectPath(profile_.path),
      base::BindOnce(&PolicyApplicator::GetProfilePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PolicyApplicator::GetProfilePropertiesError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PolicyApplicator::GetProfilePropertiesCallback(
    base::Value::Dict profile_properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Received properties for profile " << profile_.ToDebugString();
  const base::Value::List* entries =
      profile_properties.FindList(shill::kEntriesProperty);
  if (!entries) {
    LOG(ERROR) << "Profile " << profile_.ToDebugString()
               << " doesn't contain the property " << shill::kEntriesProperty;
    NotifyConfigurationHandlerAndFinish();
    return;
  }

  for (const auto& it : *entries) {
    if (!it.is_string()) {
      continue;
    }

    std::string entry_identifier = it.GetString();

    pending_get_entry_calls_.insert(entry_identifier);
    ShillProfileClient::Get()->GetEntry(
        dbus::ObjectPath(profile_.path), entry_identifier,
        base::BindOnce(&PolicyApplicator::GetEntryCallback,
                       weak_ptr_factory_.GetWeakPtr(), entry_identifier),
        base::BindOnce(&PolicyApplicator::GetEntryError,
                       weak_ptr_factory_.GetWeakPtr(), entry_identifier));
  }
  if (pending_get_entry_calls_.empty()) {
    ApplyRemainingPolicies();
  }
}

void PolicyApplicator::GetProfilePropertiesError(
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Could not retrieve properties of profile " << profile_.path
             << ": " << error_message;
  NotifyConfigurationHandlerAndFinish();
}

void PolicyApplicator::GetEntryCallback(const std::string& entry_identifier,
                                        base::Value::Dict entry_properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Received properties for entry " << entry_identifier
          << " of profile " << profile_.ToDebugString();

  base::Value::Dict onc_part = onc::TranslateShillServiceToONCPart(
      entry_properties, ::onc::ONC_SOURCE_UNKNOWN,
      &chromeos::onc::kNetworkWithStateSignature, nullptr /* network_state */);

  std::string old_guid = GetGUIDFromONCPart(onc_part);
  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(entry_properties);
  if (!ui_data) {
    VLOG(1) << "Entry " << entry_identifier << " of profile "
            << profile_.ToDebugString() << " contains no or no valid UIData.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged. It's an inconsistency if there is a GUID but no UIData, thus
    // clear the GUID just in case.
    old_guid.clear();
  }

  bool was_managed =
      ui_data && (ui_data->onc_source() == ::onc::ONC_SOURCE_DEVICE_POLICY ||
                  ui_data->onc_source() == ::onc::ONC_SOURCE_USER_POLICY);

  const base::Value::Dict* new_policy = nullptr;
  if (was_managed) {
    // If we have a GUID that might match a current policy, do a lookup using
    // that GUID at first. In particular this is necessary, as some networks
    // can't be matched to policies by properties (e.g. VPN).
    new_policy = GetByGUID(all_policies_, old_guid);
  }

  if (!new_policy) {
    // If we didn't find a policy by GUID, still a new policy might match.
    new_policy = FindMatchingPolicy(all_policies_, onc_part);
  }

  auto profile_entry_finished_callback =
      base::BindOnce(&PolicyApplicator::ProfileEntryFinished,
                     weak_ptr_factory_.GetWeakPtr(), entry_identifier);
  if (new_policy) {
    std::string new_guid = GetGUIDFromONCPart(*new_policy);
    DCHECK(!new_guid.empty());
    VLOG_IF(1, was_managed && old_guid != new_guid)
        << "Updating configuration previously managed by policy " << old_guid
        << " with new policy " << new_guid << ".";
    VLOG_IF(1, !was_managed)
        << "Applying policy " << new_guid << " to previously unmanaged "
        << "configuration.";

    ApplyOncPolicy(entry_identifier, entry_properties, std::move(ui_data),
                   old_guid, new_guid, *new_policy,
                   std::move(profile_entry_finished_callback));

    const std::string* iccid = policy_util::GetIccidFromONC(*new_policy);

    // If we detect that a managed cellular network already exists that matches
    // the policy being applied we update the preferences that are used to track
    // eSIM profiles that have been installed for managed networks to match this
    // more recent policy application.
    const std::string* name =
        new_policy->FindString(::onc::network_config::kName);
    std::optional<policy_util::SmdxActivationCode> activation_code =
        policy_util::GetSmdxActivationCodeFromONC(*new_policy);
    if (managed_cellular_pref_handler_ && iccid && name &&
        activation_code.has_value()) {
      managed_cellular_pref_handler_->AddESimMetadata(*iccid, *name,
                                                      *activation_code);
    }
    return;
  }

  if (was_managed) {
    VLOG(1) << "Removing configuration previously managed by policy "
            << old_guid << ", because the policy was removed.";

    // Remove the entry, because the network was managed but isn't anymore.
    // Note: An alternative might be to preserve the user settings, but it's
    // unclear which values originating the policy should be removed.
    DeleteEntry(entry_identifier, std::move(profile_entry_finished_callback));

    const std::string* iccid = policy_util::GetIccidFromONC(onc_part);
    if (managed_cellular_pref_handler_ && iccid) {
      managed_cellular_pref_handler_->SetPolicyMissing(*iccid);
    }
    return;
  }

  ApplyGlobalPolicyOnUnmanagedEntry(entry_identifier, entry_properties,
                                    std::move(profile_entry_finished_callback));
}

void PolicyApplicator::GetEntryError(const std::string& entry_identifier,
                                     const std::string& error_name,
                                     const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Could not retrieve entry " << entry_identifier
             << " of profile " << profile_.path << ": " << error_message;
  ProfileEntryFinished(entry_identifier);
}

void PolicyApplicator::ApplyOncPolicy(const std::string& entry_identifier,
                                      const base::Value::Dict& entry_properties,
                                      std::unique_ptr<NetworkUIData> ui_data,
                                      const std::string& old_guid,
                                      const std::string& new_guid,
                                      const base::Value::Dict& new_policy,
                                      base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool policy_guid_changed = (old_guid != new_guid);
  const bool force_reset = policy_util::AreEphemeralNetworkPoliciesEnabled() &&
                           options_.reset_recommended_managed_configs &&
                           policy_util::HasAnyRecommendedField(new_policy);
  const bool policy_contents_changed =
      base::Contains(remaining_policy_guids_, new_guid);
  remaining_policy_guids_.erase(new_guid);

  if (!policy_guid_changed && !policy_contents_changed && !force_reset) {
    VLOG(1) << "Not updating existing managed configuration with guid "
            << new_guid << " because the policy didn't change.";
    std::move(callback).Run();
    return;
  }

  const base::Value::Dict* user_settings = nullptr;
  if (!force_reset) {
    // TODO(b/260832333): We may also want to explicitly keep auth credentials
    // which are currently not part of |ui_data|.
    user_settings = ui_data ? ui_data->GetUserSettingsDictionary() : nullptr;
  }
  base::Value::Dict new_shill_properties =
      policy_util::CreateShillConfiguration(profile_, new_guid,
                                            &global_network_config_,
                                            &new_policy, user_settings);

  // Copy over the value of ICCID and EID property from old entry to new shill
  // properties since Shill requires ICCID and EID to create or update the
  // existing service.
  CopyRequiredCellularProperties(entry_properties, &new_shill_properties);

  // In order to keep implicit state of Shill like "connected successfully
  // before", keep the entry if a policy is reapplied (e.g. after reboot) or is
  // updated. However, some Shill properties are used to identify the network
  // and cannot be modified after initial configuration, so we have to delete
  // the profile entry in these cases. Also, keeping Shill's state if the
  // SSID changed might not be a good idea anyways. If the policy GUID
  // changed, or there was no policy before, we delete the entry at first to
  // ensure that no old configuration remains.
  const bool identifying_properties_match =
      shill_property_util::DoIdentifyingPropertiesMatch(new_shill_properties,
                                                        entry_properties);
  if (!policy_guid_changed && identifying_properties_match && !force_reset) {
    NET_LOG(EVENT) << "Updating previously managed configuration with the "
                   << "updated policy " << new_guid << ".";
    WriteNewShillConfiguration(std::move(new_shill_properties),
                               new_policy.Clone(), std::move(callback));
  } else {
    NET_LOG(EVENT) << "Deleting profile entry before writing new policy "
                   << new_guid
                   << " [policy_guid_changed=" << policy_guid_changed
                   << ", identifying_propreties_match="
                   << identifying_properties_match
                   << ", force_reset=" << force_reset << "]";
    // In general, old entries should at first be deleted before new
    // configurations are written to prevent inconsistencies. Therefore, we
    // delay the writing of the new config here until ~PolicyApplicator.
    // E.g. one problematic case is if a policy { {GUID=X, SSID=Y} } is
    // applied to the profile entries
    // { ENTRY1 = {GUID=X, SSID=X, USER_SETTINGS=X},
    //   ENTRY2 = {SSID=Y, ... } }.
    // At first ENTRY1 and ENTRY2 should be removed, then the new config be
    // written and the result should be:
    // { {GUID=X, SSID=Y, USER_SETTINGS=X} }
    DeleteEntry(entry_identifier,
                base::BindOnce(&PolicyApplicator::WriteNewShillConfiguration,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(new_shill_properties),
                               new_policy.Clone(), std::move(callback)));
  }
}

void PolicyApplicator::ApplyGlobalPolicyOnUnmanagedEntry(
    const std::string& entry_identifier,
    const base::Value::Dict& entry_properties,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The entry wasn't managed and doesn't match any current policy. Global
  // network settings have to be applied.

  if (options_.remove_unmanaged_configs) {
    DCHECK(policy_util::AreEphemeralNetworkPoliciesEnabled());
    NET_LOG(EVENT) << "Removing unmanaged entry " << entry_identifier
                   << " due to ephemeral networks policy.";
    DeleteEntry(entry_identifier, std::move(callback));
    return;
  }

  base::Value::Dict shill_properties_to_update;
  policy_util::SetShillPropertiesForGlobalPolicy(
      entry_properties, global_network_config_, shill_properties_to_update);
  if (shill_properties_to_update.empty()) {
    VLOG(2) << "Ignore unmanaged entry.";
    // Calling a SetProperties of Shill with an empty dictionary is a no op.
    std::move(callback).Run();
    return;
  }
  NET_LOG(EVENT) << "Apply global network config to unmanaged entry "
                 << entry_identifier << ".";
  handler_->UpdateExistingConfigurationWithPropertiesFromPolicy(
      entry_properties, std::move(shill_properties_to_update),
      std::move(callback));
}

void PolicyApplicator::DeleteEntry(const std::string& entry_identifier,
                                   base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  ShillProfileClient::Get()->DeleteEntry(
      dbus::ObjectPath(profile_.path), entry_identifier,
      std::move(split_callback.first),
      base::BindOnce(&LogErrorMessageAndInvokeCallback,
                     std::move(split_callback.second), FROM_HERE));
}

void PolicyApplicator::WriteNewShillConfiguration(
    base::Value::Dict shill_dictionary,
    base::Value::Dict policy,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ethernet (non EAP) settings, like GUID or UIData, cannot be stored per
  // user. Abort in that case.
  const std::string* type = policy.FindString(::onc::network_config::kType);
  if (type && *type == ::onc::network_type::kEthernet &&
      profile_.type() == NetworkProfile::TYPE_USER) {
    const base::Value::Dict* ethernet =
        policy.FindDict(::onc::network_config::kEthernet);
    if (ethernet) {
      const std::string* auth =
          ethernet->FindString(::onc::ethernet::kAuthentication);
      if (auth && *auth == ::onc::ethernet::kAuthenticationNone) {
        std::move(callback).Run();
        return;
      }
    }
  }

  handler_->CreateConfigurationFromPolicy(shill_dictionary,
                                          std::move(callback));
}

void PolicyApplicator::ProfileEntryFinished(
    const std::string& entry_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = pending_get_entry_calls_.find(entry_identifier);
  DCHECK(iter != pending_get_entry_calls_.end());
  pending_get_entry_calls_.erase(iter);
  if (pending_get_entry_calls_.empty())
    ApplyRemainingPolicies();
}

void PolicyApplicator::ApplyRemainingPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_get_entry_calls_.empty());

  VLOG_IF(2, !remaining_policy_guids_.empty())
      << "Create new managed network configurations in profile"
      << profile_.ToDebugString() << ".";

  // PolicyApplicator does not handle applying new cellular policies, just
  // collect these so they can be reported back to the caller.
  for (base::flat_set<std::string>::iterator it =
           remaining_policy_guids_.begin();
       it != remaining_policy_guids_.end();) {
    const std::string& guid = *it;
    const base::Value::Dict* network_policy = GetByGUID(all_policies_, guid);
    DCHECK(network_policy);
    if (policy_util::IsCellularPolicy(*network_policy)) {
      new_cellular_policy_guids_.insert(guid);
      it = remaining_policy_guids_.erase(it);
      continue;
    }
    ++it;
  }

  if (remaining_policy_guids_.empty()) {
    NotifyConfigurationHandlerAndFinish();
    return;
  }

  // All profile entries were compared to policies. |remaining_policy_guids_|
  // contains all modified policies that didn't match any entry. For these
  // remaining policies, new configurations have to be created.
  for (const std::string& guid : remaining_policy_guids_) {
    const base::Value::Dict* network_policy = GetByGUID(all_policies_, guid);
    DCHECK(network_policy);

    NET_LOG(EVENT) << "Creating new configuration managed by policy " << guid
                   << " in profile " << profile_.ToDebugString() << ".";

    base::Value::Dict shill_dictionary = policy_util::CreateShillConfiguration(
        profile_, guid, &global_network_config_, network_policy,
        /*user_settings=*/nullptr);

    handler_->CreateConfigurationFromPolicy(
        shill_dictionary,
        base::BindOnce(&PolicyApplicator::RemainingPolicyApplied,
                       weak_ptr_factory_.GetWeakPtr(), guid));
  }
}

void PolicyApplicator::RemainingPolicyApplied(const std::string& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remaining_policy_guids_.erase(guid);
  if (remaining_policy_guids_.empty()) {
    NotifyConfigurationHandlerAndFinish();
  }
}

void PolicyApplicator::NotifyConfigurationHandlerAndFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  handler_->OnPoliciesApplied(profile_, new_cellular_policy_guids_);
}

}  // namespace ash
