// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/policy_util.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_merger.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace policy_util {

const char kFakeCredential[] = "FAKE_CREDENTIAL_VPaJDV9x";

namespace {

// Removes all kFakeCredential values from sensitive fields (determined by
// onc::FieldIsCredential) of |onc_object|.
void RemoveFakeCredentials(const onc::OncValueSignature& signature,
                           base::DictionaryValue* onc_object) {
  std::vector<std::string> entries_to_remove;
  for (base::DictionaryValue::Iterator it(*onc_object); !it.IsAtEnd();
       it.Advance()) {
    base::Value* value = nullptr;
    std::string field_name = it.key();
    // We need the non-const entry to remove nested values but DictionaryValue
    // has no non-const iterator.
    onc_object->GetWithoutPathExpansion(field_name, &value);

    // If |value| is a dictionary, recurse.
    base::DictionaryValue* nested_object = nullptr;
    if (value->GetAsDictionary(&nested_object)) {
      const onc::OncFieldSignature* field_signature =
          onc::GetFieldSignature(signature, field_name);
      if (field_signature)
        RemoveFakeCredentials(*field_signature->value_signature, nested_object);
      else
        LOG(ERROR) << "ONC has unrecognized field: " << field_name;
      continue;
    }

    // If |value| is a string, check if it is a fake credential.
    std::string string_value;
    if (value->GetAsString(&string_value) &&
        onc::FieldIsCredential(signature, field_name)) {
      if (string_value == kFakeCredential) {
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

// Returns true if |policy| matches |actual_network|, which must be part of a
// ONC NetworkConfiguration. This should be the only such matching function
// within Chrome. Shill does such matching in several functions for network
// identification. For compatibility, we currently should stick to Shill's
// matching behavior.
bool IsPolicyMatching(const base::DictionaryValue& policy,
                      const base::DictionaryValue& actual_network) {
  std::string policy_type;
  policy.GetStringWithoutPathExpansion(::onc::network_config::kType,
                                       &policy_type);
  std::string actual_network_type;
  actual_network.GetStringWithoutPathExpansion(::onc::network_config::kType,
                                               &actual_network_type);
  if (policy_type != actual_network_type)
    return false;

  if (actual_network_type == ::onc::network_type::kEthernet) {
    const base::DictionaryValue* policy_ethernet = nullptr;
    policy.GetDictionaryWithoutPathExpansion(::onc::network_config::kEthernet,
                                             &policy_ethernet);
    const base::DictionaryValue* actual_ethernet = nullptr;
    actual_network.GetDictionaryWithoutPathExpansion(
        ::onc::network_config::kEthernet, &actual_ethernet);
    if (!policy_ethernet || !actual_ethernet)
      return false;

    std::string policy_auth;
    policy_ethernet->GetStringWithoutPathExpansion(
        ::onc::ethernet::kAuthentication, &policy_auth);
    std::string actual_auth;
    actual_ethernet->GetStringWithoutPathExpansion(
        ::onc::ethernet::kAuthentication, &actual_auth);
    return policy_auth == actual_auth;
  } else if (actual_network_type == ::onc::network_type::kWiFi) {
    const base::DictionaryValue* policy_wifi = nullptr;
    policy.GetDictionaryWithoutPathExpansion(::onc::network_config::kWiFi,
                                             &policy_wifi);
    const base::DictionaryValue* actual_wifi = nullptr;
    actual_network.GetDictionaryWithoutPathExpansion(
        ::onc::network_config::kWiFi, &actual_wifi);
    if (!policy_wifi || !actual_wifi)
      return false;

    std::string policy_ssid;
    policy_wifi->GetStringWithoutPathExpansion(::onc::wifi::kHexSSID,
                                               &policy_ssid);
    std::string actual_ssid;
    actual_wifi->GetStringWithoutPathExpansion(::onc::wifi::kHexSSID,
                                               &actual_ssid);
    return (policy_ssid == actual_ssid);
  }
  return false;
}

// Returns true if AutoConnect is enabled by |policy| (as mandatory or
// recommended setting). Otherwise and on error returns false.
bool IsAutoConnectEnabledInPolicy(const base::DictionaryValue& policy) {
  std::string type;
  policy.GetStringWithoutPathExpansion(::onc::network_config::kType, &type);

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

  const base::DictionaryValue* network_dict = nullptr;
  policy.GetDictionaryWithoutPathExpansion(network_dict_key, &network_dict);
  if (!network_dict) {
    LOG(ERROR) << "ONC doesn't contain a " << network_dict_key
               << " dictionary.";
    return false;
  }

  bool autoconnect = false;
  network_dict->GetBooleanWithoutPathExpansion(autoconnect_key, &autoconnect);
  return autoconnect;
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

void ApplyGlobalAutoconnectPolicy(
    NetworkProfile::Type profile_type,
    base::DictionaryValue* augmented_onc_network) {
  std::string type;
  augmented_onc_network->GetStringWithoutPathExpansion(
      ::onc::network_config::kType, &type);
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

std::unique_ptr<base::DictionaryValue> CreateManagedONC(
    const base::DictionaryValue* global_policy,
    const base::DictionaryValue* network_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* active_settings,
    const NetworkProfile* profile) {
  const base::DictionaryValue* user_policy = nullptr;
  const base::DictionaryValue* device_policy = nullptr;
  const base::DictionaryValue* nonshared_user_settings = nullptr;
  const base::DictionaryValue* shared_user_settings = nullptr;

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
  std::unique_ptr<base::DictionaryValue> augmented_onc_network =
      onc::MergeSettingsAndPoliciesToAugmented(
          onc::kNetworkConfigurationSignature, user_policy, device_policy,
          nonshared_user_settings, shared_user_settings, active_settings);

  // If present, apply the Autoconnect policy only to networks that are not
  // managed by policy.
  if (!network_policy && global_policy && profile) {
    bool allow_only_policy_autoconnect = false;
    global_policy->GetBooleanWithoutPathExpansion(
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
        &allow_only_policy_autoconnect);
    if (allow_only_policy_autoconnect) {
      ApplyGlobalAutoconnectPolicy(profile->type(),
                                   augmented_onc_network.get());
    }
  }

  return augmented_onc_network;
}

void SetShillPropertiesForGlobalPolicy(
    const base::DictionaryValue& shill_dictionary,
    const base::DictionaryValue& global_network_policy,
    base::DictionaryValue* shill_properties_to_update) {
  // kAllowOnlyPolicyNetworksToAutoconnect is currently the only global config.

  std::string type;
  shill_dictionary.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  if (NetworkTypePattern::Ethernet().MatchesType(type))
    return;  // Autoconnect for Ethernet cannot be configured.

  // By default all networks are allowed to autoconnect.
  bool only_policy_autoconnect = false;
  global_network_policy.GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      &only_policy_autoconnect);
  if (!only_policy_autoconnect)
    return;

  bool old_autoconnect = false;
  if (shill_dictionary.GetBooleanWithoutPathExpansion(
          shill::kAutoConnectProperty, &old_autoconnect) &&
      !old_autoconnect) {
    // Autoconnect is already explicitly disabled. No need to set it again.
    return;
  }

  // If autoconnect is not explicitly set yet, it might automatically be enabled
  // by Shill. To prevent that, disable it explicitly.
  shill_properties_to_update->SetKey(shill::kAutoConnectProperty,
                                     base::Value(false));
}

std::unique_ptr<base::DictionaryValue> CreateShillConfiguration(
    const NetworkProfile& profile,
    const std::string& guid,
    const base::DictionaryValue* global_policy,
    const base::DictionaryValue* network_policy,
    const base::DictionaryValue* user_settings) {
  std::unique_ptr<base::DictionaryValue> effective;
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
    effective.reset(user_settings->DeepCopy());
    // TODO(pneubeck): change to source ONC_SOURCE_USER
    onc_source = ::onc::ONC_SOURCE_NONE;
  } else {
    NOTREACHED();
  }

  RemoveFakeCredentials(onc::kNetworkConfigurationSignature, effective.get());

  effective->SetKey(::onc::network_config::kGUID, base::Value(guid));

  // Remove irrelevant fields.
  onc::Normalizer normalizer(true /* remove recommended fields */);
  effective = normalizer.NormalizeObject(&onc::kNetworkConfigurationSignature,
                                         *effective);

  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                     *effective));

  shill_dictionary->SetKey(shill::kProfileProperty, base::Value(profile.path));

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
    shill_dictionary->SetKey(shill::kManagedCredentialsProperty,
                             base::Value(true));
  }

  if (!network_policy && global_policy) {
    // The network isn't managed. Global network policies have to be applied.
    SetShillPropertiesForGlobalPolicy(*shill_dictionary, *global_policy,
                                      shill_dictionary.get());
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
        shill_dictionary->FindBoolKey(shill::kSaveCredentialsProperty)
            .value_or(true);
    const std::string credential_mask =
        saving_credentials ? kFakeCredential : std::string();
    std::unique_ptr<base::Value> sanitized_user_settings =
        base::Value::ToUniquePtrValue(
            onc::MaskCredentialsInOncObject(onc::kNetworkConfigurationSignature,
                                            *user_settings, credential_mask));
    ui_data->SetUserSettingsDictionary(std::move(sanitized_user_settings));
  }

  shill_property_util::SetUIData(*ui_data, shill_dictionary.get());

  VLOG(2) << "Created Shill properties: " << *shill_dictionary;

  return shill_dictionary;
}

const base::DictionaryValue* FindMatchingPolicy(
    const GuidToPolicyMap& policies,
    const base::DictionaryValue& actual_network) {
  for (auto it = policies.begin(); it != policies.end(); ++it) {
    if (IsPolicyMatching(*it->second, actual_network))
      return it->second.get();
  }
  return nullptr;
}

}  // namespace policy_util

}  // namespace chromeos
