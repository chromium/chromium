// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/policy_util.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/onc/onc_merger.h"
#include "chromeos/ash/components/network/onc/onc_normalizer.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::policy_util {

const char kFakeCredential[] = "FAKE_CREDENTIAL_VPaJDV9x";

namespace {

std::string GetString(const base::Value& dict, const char* key) {
  DCHECK(dict.is_dict());
  const std::string* value = dict.FindStringKey(key);
  return value ? *value : std::string();
}

// Removes all kFakeCredential values from sensitive fields (determined by
// onc::FieldIsCredential) of |onc_object|.
void RemoveFakeCredentials(const chromeos::onc::OncValueSignature& signature,
                           base::Value* onc_object) {
  std::vector<std::string> entries_to_remove;
  for (auto iter : onc_object->DictItems()) {
    std::string field_name = iter.first;
    base::Value* value = &iter.second;

    // If |value| is a dictionary, recurse.
    if (value->is_dict()) {
      const chromeos::onc::OncFieldSignature* field_signature =
          chromeos::onc::GetFieldSignature(signature, field_name);
      if (field_signature)
        RemoveFakeCredentials(*field_signature->value_signature, value);
      else
        LOG(ERROR) << "ONC has unrecognized field: " << field_name;
      continue;
    }

    // If |value| is a string, check if it is a fake credential.
    if (value->is_string() &&
        chromeos::onc::FieldIsCredential(signature, field_name)) {
      if (value->GetString() == kFakeCredential) {
        // The value wasn't modified by the UI, thus we remove the field to keep
        // the existing value that is stored in Shill.
        entries_to_remove.push_back(field_name);
      }
      // Otherwise, the value is set and modified by the UI, thus we keep that
      // value to overwrite whatever is stored in Shill.
    }
  }
  for (auto field_name : entries_to_remove)
    onc_object->RemoveKey(field_name);
}

// Returns true if AutoConnect is enabled by |policy| (as mandatory or
// recommended setting). Otherwise and on error returns false.
bool IsAutoConnectEnabledInPolicy(const base::Value& policy) {
  std::string type = GetString(policy, ::onc::network_config::kType);

  std::string autoconnect_key;
  std::string network_dict_key;
  if (type == ::onc::network_type::kWiFi) {
    network_dict_key = ::onc::network_config::kWiFi;
    autoconnect_key = ::onc::wifi::kAutoConnect;
  } else if (type == ::onc::network_type::kVPN) {
    network_dict_key = ::onc::network_config::kVPN;
    autoconnect_key = ::onc::vpn::kAutoConnect;
  } else {
    VLOG(2) << "Network type without autoconnect property.";
    return false;
  }

  const base::Value* network_dict = policy.FindDictKey(network_dict_key);
  if (!network_dict) {
    LOG(ERROR) << "ONC doesn't contain a " << network_dict_key
               << " dictionary.";
    return false;
  }

  return network_dict->FindBoolKey(autoconnect_key).value_or(false);
}

base::Value* GetOrCreateNestedDictionary(const std::string& key1,
                                         const std::string& key2,
                                         base::Value* dict) {
  base::Value* inner_dict =
      dict->FindPathOfType({key1, key2}, base::Value::Type::DICTIONARY);
  if (inner_dict)
    return inner_dict;
  return dict->SetPath({key1, key2},
                       base::Value(base::Value::Type::DICTIONARY));
}

void ApplyGlobalAutoconnectPolicy(NetworkProfile::Type profile_type,
                                  base::Value* augmented_onc_network) {
  std::string type =
      GetString(*augmented_onc_network, ::onc::network_config::kType);
  if (type.empty()) {
    LOG(ERROR) << "ONC dictionary with no Type.";
    return;
  }

  // Managed dictionaries don't contain empty dictionaries (see onc_merger.cc),
  // so add the Autoconnect dictionary in case Shill didn't report a value.
  base::Value* auto_connect_dictionary = nullptr;
  if (type == ::onc::network_type::kWiFi) {
    auto_connect_dictionary = GetOrCreateNestedDictionary(
        ::onc::network_config::kWiFi, ::onc::wifi::kAutoConnect,
        augmented_onc_network);
  } else if (type == ::onc::network_type::kVPN) {
    auto_connect_dictionary = GetOrCreateNestedDictionary(
        ::onc::network_config::kVPN, ::onc::vpn::kAutoConnect,
        augmented_onc_network);
  } else {
    return;  // Network type without auto-connect property.
  }

  std::string policy_source;
  switch (profile_type) {
    case NetworkProfile::TYPE_USER:
      policy_source = ::onc::kAugmentationUserPolicy;
      break;
    case NetworkProfile::TYPE_SHARED:
      policy_source = ::onc::kAugmentationDevicePolicy;
      break;
  }
  DCHECK(!policy_source.empty());

  auto_connect_dictionary->SetKey(policy_source, base::Value(false));
  auto_connect_dictionary->SetKey(::onc::kAugmentationEffectiveSetting,
                                  base::Value(policy_source));
}

}  // namespace

base::Value CreateManagedONC(const base::Value* global_policy,
                             const base::Value* network_policy,
                             const base::Value* user_settings,
                             const base::Value* active_settings,
                             const NetworkProfile* profile) {
  const base::Value* user_policy = nullptr;
  const base::Value* device_policy = nullptr;
  const base::Value* nonshared_user_settings = nullptr;
  const base::Value* shared_user_settings = nullptr;

  if (profile) {
    switch (profile->type()) {
      case NetworkProfile::TYPE_SHARED:
        device_policy = network_policy;
        shared_user_settings = user_settings;
        break;
      case NetworkProfile::TYPE_USER:
        user_policy = network_policy;
        nonshared_user_settings = user_settings;
        break;
    }
  }

  // This call also removes credentials from policies.
  base::Value augmented_onc_network = onc::MergeSettingsAndPoliciesToAugmented(
      chromeos::onc::kNetworkConfigurationSignature, user_policy, device_policy,
      nonshared_user_settings, shared_user_settings, active_settings);

  // If present, apply the Autoconnect policy only to networks that are not
  // managed by policy.
  if (!network_policy && global_policy && profile) {
    bool allow_only_policy_autoconnect =
        global_policy
            ->FindBoolKey(::onc::global_network_config::
                              kAllowOnlyPolicyNetworksToAutoconnect)
            .value_or(false);
    if (allow_only_policy_autoconnect) {
      ApplyGlobalAutoconnectPolicy(profile->type(), &augmented_onc_network);
    }
  }

  return augmented_onc_network;
}

// Ensures that |user_settings| contains a GUID `guid` for Ethernet
// policy-managed networks.
// Background:
// In Chrome OS M-105 and older, it was possible to end up in a state that has
// a different GUID in policy data and in the service's UIData dictionary.
// This leads to issues in the UI layer, so fix up the GUID in UIData if it is
// encountered.
void FixupEthernetUIDataGUID(const base::Value::Dict& new_policy,
                             const std::string& guid,
                             base::Value* user_settings) {
  DCHECK(user_settings);
  const std::string* type = new_policy.FindString(::onc::network_config::kType);
  if (!type || *type != ::onc::network_type::kEthernet)
    return;

  std::string* ui_data_guid =
      user_settings->GetDict().FindString(::onc::network_config::kGUID);
  if (!ui_data_guid)
    return;
  if (*ui_data_guid != guid) {
    LOG(ERROR) << "Fixing Ethernet UIData GUID";
    *ui_data_guid = guid;
  }
}

void SetShillPropertiesForGlobalPolicy(
    const base::Value& shill_dictionary,
    const base::Value& global_network_policy,
    base::Value* shill_properties_to_update) {
  // kAllowOnlyPolicyNetworksToAutoconnect is currently the only global config.

  std::string type = GetString(shill_dictionary, shill::kTypeProperty);
  if (NetworkTypePattern::Ethernet().MatchesType(type))
    return;  // Autoconnect for Ethernet cannot be configured.

  // By default all networks are allowed to autoconnect.
  bool only_policy_autoconnect =
      global_network_policy
          .FindBoolKey(::onc::global_network_config::
                           kAllowOnlyPolicyNetworksToAutoconnect)
          .value_or(false);
  if (!only_policy_autoconnect)
    return;

  bool old_autoconnect =
      shill_dictionary.FindBoolKey(shill::kAutoConnectProperty).value_or(false);
  if (!old_autoconnect) {
    // Autoconnect is already explicitly disabled. No need to set it again.
    return;
  }

  // If autoconnect is not explicitly set yet, it might automatically be enabled
  // by Shill. To prevent that, disable it explicitly.
  shill_properties_to_update->SetKey(shill::kAutoConnectProperty,
                                     base::Value(false));
}

base::Value CreateShillConfiguration(const NetworkProfile& profile,
                                     const std::string& guid,
                                     const base::Value* global_policy,
                                     const base::Value* network_policy,
                                     const base::Value* user_settings) {
  base::Value effective;
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  if (network_policy) {
    switch (profile.type()) {
      case NetworkProfile::TYPE_SHARED:
        effective = onc::MergeSettingsAndPoliciesToEffective(
            nullptr,         // no user policy
            network_policy,  // device policy
            nullptr,         // no user settings
            user_settings);  // shared settings
        onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
        break;
      case NetworkProfile::TYPE_USER:
        effective = onc::MergeSettingsAndPoliciesToEffective(
            network_policy,  // user policy
            nullptr,         // no device policy
            user_settings,   // user settings
            nullptr);        // no shared settings
        onc_source = ::onc::ONC_SOURCE_USER_POLICY;
        break;
    }
    DCHECK(onc_source != ::onc::ONC_SOURCE_NONE);
  } else if (user_settings) {
    effective = user_settings->Clone();
    // TODO(pneubeck): change to source ONC_SOURCE_USER
    onc_source = ::onc::ONC_SOURCE_NONE;
  } else {
    NOTREACHED();
  }

  RemoveFakeCredentials(chromeos::onc::kNetworkConfigurationSignature,
                        &effective);

  effective.SetKey(::onc::network_config::kGUID, base::Value(guid));

  // Remove irrelevant fields.
  onc::Normalizer normalizer(true /* remove recommended fields */);
  effective = normalizer.NormalizeObject(
      &chromeos::onc::kNetworkConfigurationSignature, effective);

  base::Value shill_dictionary = onc::TranslateONCObjectToShill(
      &chromeos::onc::kNetworkConfigurationSignature, effective);
  shill_dictionary.SetKey(shill::kProfileProperty, base::Value(profile.path));

  // If AutoConnect is enabled by policy, set the ManagedCredentials property to
  // indicate to Shill that this network can be used for autoconnect even
  // without a manual and successful connection attempt.
  // Note that this is only an indicator for the administrator's true intention,
  // i.e. when the administrator enables AutoConnect, we assume that the network
  // is indeed connectable.
  // Ideally, we would know whether the (policy) provided credentials are
  // complete and only set ManagedCredentials in that case.
  if (network_policy && IsAutoConnectEnabledInPolicy(*network_policy)) {
    VLOG(1) << "Enable ManagedCredentials for managed network with GUID "
            << guid;
    shill_dictionary.SetKey(shill::kManagedCredentialsProperty,
                            base::Value(true));
  }

  if (!network_policy && global_policy) {
    // The network isn't managed. Global network policies have to be applied.
    SetShillPropertiesForGlobalPolicy(shill_dictionary, *global_policy,
                                      &shill_dictionary);
  }

  std::unique_ptr<NetworkUIData> ui_data(
      NetworkUIData::CreateFromONC(onc_source));

  if (user_settings) {
    // Shill doesn't know that sensitive data is contained in the UIData
    // property and might write it into logs or other insecure places. Thus, we
    // have to remove or mask credentials.
    //
    // Shill's GetProperties doesn't return credentials. Masking credentials
    // instead of just removing them, allows remembering if a credential is set
    // or not.
    //
    // If we're not saving credentials, explicitly set credentials in UIData to
    // empty string so the UI will display empty text fields for them the next
    // time they're viewed (instead of masked-out-placeholders, which would
    // suggest that a credential has been saved).
    const bool saving_credentials =
        shill_dictionary.FindBoolKey(shill::kSaveCredentialsProperty)
            .value_or(true);
    const std::string credential_mask =
        saving_credentials ? kFakeCredential : std::string();
    base::Value sanitized_user_settings =
        chromeos::onc::MaskCredentialsInOncObject(
            chromeos::onc::kNetworkConfigurationSignature, *user_settings,
            credential_mask);

    if (network_policy) {
      FixupEthernetUIDataGUID(network_policy->GetDict(), guid,
                              &sanitized_user_settings);
    }

    ui_data->SetUserSettingsDictionary(std::move(sanitized_user_settings));
  }

  shill_property_util::SetUIDataAndSource(*ui_data, &shill_dictionary);
  shill_property_util::SetRandomMACPolicy(ui_data->onc_source(),
                                          &shill_dictionary);

  VLOG(2) << "Created Shill properties: " << shill_dictionary;

  return shill_dictionary;
}

bool IsPolicyMatching(const base::Value& policy,
                      const base::Value& actual_network) {
  std::string policy_type = GetString(policy, ::onc::network_config::kType);
  std::string actual_network_type =
      GetString(actual_network, ::onc::network_config::kType);
  if (policy_type != actual_network_type)
    return false;

  if (actual_network_type == ::onc::network_type::kEthernet) {
    const base::Value* policy_ethernet =
        policy.FindDictKey(::onc::network_config::kEthernet);
    const base::Value* actual_ethernet =
        actual_network.FindDictKey(::onc::network_config::kEthernet);
    if (!policy_ethernet || !actual_ethernet)
      return false;

    std::string policy_auth =
        GetString(*policy_ethernet, ::onc::ethernet::kAuthentication);
    std::string actual_auth =
        GetString(*actual_ethernet, ::onc::ethernet::kAuthentication);
    return policy_auth == actual_auth;
  }

  if (actual_network_type == ::onc::network_type::kWiFi) {
    const base::Value* policy_wifi =
        policy.FindDictKey(::onc::network_config::kWiFi);
    const base::Value* actual_wifi =
        actual_network.FindDictKey(::onc::network_config::kWiFi);
    if (!policy_wifi || !actual_wifi)
      return false;

    std::string policy_ssid = GetString(*policy_wifi, ::onc::wifi::kHexSSID);
    std::string actual_ssid = GetString(*actual_wifi, ::onc::wifi::kHexSSID);
    return (policy_ssid == actual_ssid);
  }

  if (actual_network_type == ::onc::network_type::kCellular) {
    const base::Value* policy_cellular =
        policy.FindDictKey(::onc::network_config::kCellular);
    const base::Value* actual_cellular =
        actual_network.FindDictKey(::onc::network_config::kCellular);
    if (!policy_cellular || !actual_cellular)
      return false;

    std::string policy_iccid =
        GetString(*policy_cellular, ::onc::cellular::kICCID);
    std::string actual_iccid =
        GetString(*actual_cellular, ::onc::cellular::kICCID);
    return (policy_iccid == actual_iccid && !policy_iccid.empty());
  }

  return false;
}

bool IsCellularPolicy(const base::Value& onc_config) {
  const std::string* type =
      onc_config.FindStringKey(::onc::network_config::kType);
  return type && *type == ::onc::network_type::kCellular;
}

const std::string* GetIccidFromONC(const base::Value& onc_config) {
  if (!IsCellularPolicy(onc_config))
    return nullptr;

  const base::Value* cellular_dict =
      onc_config.FindDictKey(::onc::network_config::kCellular);
  if (!cellular_dict || !cellular_dict->is_dict())
    return nullptr;

  return cellular_dict->FindStringKey(::onc::cellular::kICCID);
}

const std::string* GetSMDPAddressFromONC(const base::Value& onc_config) {
  const std::string* type =
      onc_config.FindStringKey(::onc::network_config::kType);
  const base::Value* cellular_dict =
      onc_config.FindKey(::onc::network_config::kCellular);
  const std::string* smdp_address = nullptr;

  if (type && (*type == ::onc::network_type::kCellular) && cellular_dict &&
      cellular_dict->is_dict())
    smdp_address = cellular_dict->FindStringKey(::onc::cellular::kSMDPAddress);

  return smdp_address;
}

}  // namespace ash::policy_util
