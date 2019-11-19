// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/policy_applicator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/policy_util.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void LogErrorMessageAndInvokeCallback(base::RepeatingClosure callback,
                                      const base::Location& from_where,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  LOG(ERROR) << from_where.ToString() << ": " << error_message;
  callback.Run();
}

const base::DictionaryValue* GetByGUID(
    const PolicyApplicator::GuidToPolicyMap& policies,
    const std::string& guid) {
  auto it = policies.find(guid);
  if (it == policies.end())
    return nullptr;
  return it->second.get();
}

// Returns the GUID property from |onc_part|, or an empty string if no GUID was
// present.
std::string GetGUIDFromONCPart(const base::Value& onc_part) {
  const base::Value* guid_value = onc_part.FindKeyOfType(
      ::onc::network_config::kGUID, base::Value::Type::STRING);
  if (!guid_value)
    return std::string();
  return guid_value->GetString();
}

// Special service name in shill remembering settings across ethernet services.
// Chrome should not attempt to configure / delete this.
const char kEthernetAnyService[] = "ethernet_any";

}  // namespace

PolicyApplicator::PolicyApplicator(
    const NetworkProfile& profile,
    const GuidToPolicyMap& all_policies,
    const base::DictionaryValue& global_network_config,
    ConfigurationHandler* handler,
    std::set<std::string>* modified_policy_guids)
    : handler_(handler), profile_(profile) {
  global_network_config_.MergeDictionary(&global_network_config);
  remaining_policy_guids_.swap(*modified_policy_guids);
  for (const auto& policy_pair : all_policies) {
    all_policies_.insert(std::make_pair(policy_pair.first,
                                        policy_pair.second->CreateDeepCopy()));
  }
}

PolicyApplicator::~PolicyApplicator() {
  VLOG(1) << "Destroying PolicyApplicator for " << profile_.userhash;
}

void PolicyApplicator::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShillProfileClient::Get()->GetProperties(
      dbus::ObjectPath(profile_.path),
      base::Bind(&PolicyApplicator::GetProfilePropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PolicyApplicator::GetProfilePropertiesError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PolicyApplicator::GetProfilePropertiesCallback(
    const base::DictionaryValue& profile_properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Received properties for profile " << profile_.ToDebugString();
  const base::ListValue* entries = nullptr;
  if (!profile_properties.GetListWithoutPathExpansion(
           shill::kEntriesProperty, &entries)) {
    LOG(ERROR) << "Profile " << profile_.ToDebugString()
               << " doesn't contain the property "
               << shill::kEntriesProperty;
    NotifyConfigurationHandlerAndFinish();
    return;
  }

  for (base::ListValue::const_iterator it = entries->begin();
       it != entries->end(); ++it) {
    std::string entry;
    it->GetAsString(&entry);

    // Skip "ethernet_any", as this is used by shill internally to persist
    // ethernet settings and the policy application logic should not mess with
    // it.
    if (entry == kEthernetAnyService)
      continue;

    pending_get_entry_calls_.insert(entry);
    ShillProfileClient::Get()->GetEntry(
        dbus::ObjectPath(profile_.path), entry,
        base::Bind(&PolicyApplicator::GetEntryCallback,
                   weak_ptr_factory_.GetWeakPtr(), entry),
        base::Bind(&PolicyApplicator::GetEntryError,
                   weak_ptr_factory_.GetWeakPtr(), entry));
  }
  if (pending_get_entry_calls_.empty())
    ApplyRemainingPolicies();
}

void PolicyApplicator::GetProfilePropertiesError(
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Could not retrieve properties of profile " << profile_.path
             << ": " << error_message;
  NotifyConfigurationHandlerAndFinish();
}

void PolicyApplicator::GetEntryCallback(
    const std::string& entry,
    const base::DictionaryValue& entry_properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Received properties for entry " << entry << " of profile "
          << profile_.ToDebugString();

  std::unique_ptr<base::DictionaryValue> onc_part(
      onc::TranslateShillServiceToONCPart(
          entry_properties, ::onc::ONC_SOURCE_UNKNOWN,
          &onc::kNetworkWithStateSignature, nullptr /* network_state */));

  std::string old_guid = GetGUIDFromONCPart(*onc_part);
  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(entry_properties);
  if (!ui_data) {
    VLOG(1) << "Entry " << entry << " of profile " << profile_.ToDebugString()
            << " contains no or no valid UIData.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged. It's an inconsistency if there is a GUID but no UIData, thus
    // clear the GUID just in case.
    old_guid.clear();
  }

  bool was_managed =
      ui_data && (ui_data->onc_source() == ::onc::ONC_SOURCE_DEVICE_POLICY ||
                  ui_data->onc_source() == ::onc::ONC_SOURCE_USER_POLICY);

  const base::Value* new_policy = nullptr;
  if (was_managed) {
    // If we have a GUID that might match a current policy, do a lookup using
    // that GUID at first. In particular this is necessary, as some networks
    // can't be matched to policies by properties (e.g. VPN).
    new_policy = GetByGUID(all_policies_, old_guid);
  }

  if (!new_policy) {
    // If we didn't find a policy by GUID, still a new policy might match.
    new_policy = policy_util::FindMatchingPolicy(all_policies_, *onc_part);
  }

  auto profile_entry_finished_callback =
      base::BindOnce(&PolicyApplicator::ProfileEntryFinished,
                     weak_ptr_factory_.GetWeakPtr(), entry);
  if (new_policy) {
    std::string new_guid = GetGUIDFromONCPart(*new_policy);
    DCHECK(!new_guid.empty());
    VLOG_IF(1, was_managed && old_guid != new_guid)
        << "Updating configuration previously managed by policy " << old_guid
        << " with new policy " << new_guid << ".";
    VLOG_IF(1, !was_managed) << "Applying policy " << new_guid
                             << " to previously unmanaged "
                             << "configuration.";

    ApplyNewPolicy(entry, entry_properties, std::move(ui_data), old_guid,
                   new_guid, *new_policy,
                   std::move(profile_entry_finished_callback));
    return;
  }

  if (was_managed) {
    VLOG(1) << "Removing configuration previously managed by policy "
            << old_guid << ", because the policy was removed.";

    // Remove the entry, because the network was managed but isn't anymore.
    // Note: An alternative might be to preserve the user settings, but it's
    // unclear which values originating the policy should be removed.
    DeleteEntry(entry, std::move(profile_entry_finished_callback));
    return;
  }

  ApplyGlobalPolicyOnUnmanagedEntry(entry, entry_properties,
                                    std::move(profile_entry_finished_callback));
}

void PolicyApplicator::GetEntryError(const std::string& entry,
                                     const std::string& error_name,
                                     const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Could not retrieve entry " << entry << " of profile "
             << profile_.path << ": " << error_message;
  ProfileEntryFinished(entry);
}

void PolicyApplicator::ApplyNewPolicy(const std::string& entry,
                                      const base::Value& entry_properties,
                                      std::unique_ptr<NetworkUIData> ui_data,
                                      const std::string& old_guid,
                                      const std::string& new_guid,
                                      const base::Value& new_policy,
                                      base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (old_guid == new_guid &&
      remaining_policy_guids_.find(new_guid) == remaining_policy_guids_.end()) {
    VLOG(1) << "Not updating existing managed configuration with guid "
            << new_guid << " because the policy didn't change.";
    std::move(callback).Run();
    return;
  }
  remaining_policy_guids_.erase(new_guid);

  const base::DictionaryValue* new_policy_as_dict = nullptr;
  new_policy.GetAsDictionary(&new_policy_as_dict);
  DCHECK(new_policy_as_dict);

  const base::DictionaryValue* entry_properties_as_dict = nullptr;
  entry_properties.GetAsDictionary(&entry_properties_as_dict);
  DCHECK(entry_properties_as_dict);

  const base::DictionaryValue* user_settings =
      ui_data ? ui_data->GetUserSettingsDictionary() : nullptr;
  std::unique_ptr<base::DictionaryValue> new_shill_properties =
      policy_util::CreateShillConfiguration(profile_, new_guid,
                                            &global_network_config_,
                                            new_policy_as_dict, user_settings);
  // A new policy has to be applied to this profile entry. In order to keep
  // implicit state of Shill like "connected successfully before", keep the
  // entry if a policy is reapplied (e.g. after reboot) or is updated.
  // However, some Shill properties are used to identify the network and
  // cannot be modified after initial configuration, so we have to delete
  // the profile entry in these cases. Also, keeping Shill's state if the
  // SSID changed might not be a good idea anyways. If the policy GUID
  // changed, or there was no policy before, we delete the entry at first to
  // ensure that no old configuration remains.
  if (old_guid == new_guid &&
      shill_property_util::DoIdentifyingPropertiesMatch(
          *new_shill_properties, *entry_properties_as_dict)) {
    VLOG(1) << "Updating previously managed configuration with the "
            << "updated policy " << new_guid << ".";
    WriteNewShillConfiguration(new_shill_properties->Clone(),
                               new_policy.Clone(), std::move(callback));
  } else {
    VLOG(1) << "Deleting profile entry before writing new policy " << new_guid
            << " because of identifying properties changed.";
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
    DeleteEntry(
        entry, base::BindOnce(&PolicyApplicator::WriteNewShillConfiguration,
                              weak_ptr_factory_.GetWeakPtr(),
                              new_shill_properties->Clone(), new_policy.Clone(),
                              std::move(callback)));
  }
}

void PolicyApplicator::ApplyGlobalPolicyOnUnmanagedEntry(
    const std::string& entry,
    const base::DictionaryValue& entry_properties,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The entry wasn't managed and doesn't match any current policy. Global
  // network settings have to be applied.
  base::DictionaryValue shill_properties_to_update;
  policy_util::SetShillPropertiesForGlobalPolicy(
      entry_properties, global_network_config_, &shill_properties_to_update);
  if (shill_properties_to_update.empty()) {
    VLOG(2) << "Ignore unmanaged entry.";
    // Calling a SetProperties of Shill with an empty dictionary is a no op.
    std::move(callback).Run();
    return;
  }
  VLOG(2) << "Apply global network config to unmanaged entry.";
  const base::DictionaryValue* entry_properties_as_dict = nullptr;
  entry_properties.GetAsDictionary(&entry_properties_as_dict);
  DCHECK(entry_properties_as_dict);
  handler_->UpdateExistingConfigurationWithPropertiesFromPolicy(
      *entry_properties_as_dict, shill_properties_to_update,
      std::move(callback));
}

void PolicyApplicator::DeleteEntry(const std::string& entry,
                                   base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RepeatingClosure adapted_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  ShillProfileClient::Get()->DeleteEntry(
      dbus::ObjectPath(profile_.path), entry, adapted_callback,
      base::BindRepeating(&LogErrorMessageAndInvokeCallback, adapted_callback,
                          FROM_HERE));
}

void PolicyApplicator::WriteNewShillConfiguration(base::Value shill_dictionary,
                                                  base::Value policy,
                                                  base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ethernet (non EAP) settings, like GUID or UIData, cannot be stored per
  // user. Abort in that case.
  const std::string* type = policy.FindStringKey(::onc::network_config::kType);
  if (type && *type == ::onc::network_type::kEthernet &&
      profile_.type() == NetworkProfile::TYPE_USER) {
    const base::Value* ethernet = policy.FindKeyOfType(
        ::onc::network_config::kEthernet, base::Value::Type::DICTIONARY);
    if (ethernet) {
      const std::string* auth =
          ethernet->FindStringKey(::onc::ethernet::kAuthentication);
      if (auth && *auth == ::onc::ethernet::kAuthenticationNone) {
        std::move(callback).Run();
        return;
      }
    }
  }

  const base::DictionaryValue* shill_dictionary_as_dict;
  shill_dictionary.GetAsDictionary(&shill_dictionary_as_dict);
  handler_->CreateConfigurationFromPolicy(*shill_dictionary_as_dict,
                                          std::move(callback));
}

void PolicyApplicator::ProfileEntryFinished(const std::string& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = pending_get_entry_calls_.find(entry);
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

  if (remaining_policy_guids_.empty()) {
    NotifyConfigurationHandlerAndFinish();
    return;
  }

  // All profile entries were compared to policies. |remaining_policy_guids_|
  // contains all modified policies that didn't match any entry. For these
  // remaining policies, new configurations have to be created.
  for (std::set<std::string>::iterator it = remaining_policy_guids_.begin();
       it != remaining_policy_guids_.end(); ++it) {
    const base::DictionaryValue* network_policy = GetByGUID(all_policies_, *it);
    DCHECK(network_policy);

    VLOG(1) << "Creating new configuration managed by policy " << *it
            << " in profile " << profile_.ToDebugString() << ".";

    std::unique_ptr<base::DictionaryValue> shill_dictionary =
        policy_util::CreateShillConfiguration(
            profile_, *it, &global_network_config_, network_policy,
            nullptr /* no user settings */);

    handler_->CreateConfigurationFromPolicy(
        *shill_dictionary,
        base::BindOnce(&PolicyApplicator::RemainingPolicyApplied,
                       weak_ptr_factory_.GetWeakPtr(), *it /* entry */));
  }
}

void PolicyApplicator::RemainingPolicyApplied(const std::string& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remaining_policy_guids_.erase(entry);
  if (remaining_policy_guids_.empty()) {
    NotifyConfigurationHandlerAndFinish();
  }
}

void PolicyApplicator::NotifyConfigurationHandlerAndFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  handler_->OnPoliciesApplied(profile_);
}

}  // namespace chromeos
