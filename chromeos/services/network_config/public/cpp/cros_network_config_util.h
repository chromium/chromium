// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos::network_config {

// Key names aligned with the field names defined in
// `cros_network_config.mojom.WiFiConfigProperties` mojo interface but with JS
// naming style (e.g. type_config => typeConfig).
extern const char kMojoKeySecurity[];
extern const char kMojoKeySsid[];
extern const char kMojoKeyPassphrase[];
extern const char kMojoKeyEapInner[];
extern const char kMojoKeyEapOuter[];
extern const char kMojoKeyEapIdentity[];
extern const char kMojoKeyEapAnonymousIdentity[];
extern const char kMojoKeyEapPassword[];
extern const char kMojoKeyEap[];
extern const char kMojoKeyWifi[];
extern const char kMojoKeyTypeConfig[];

struct ManagedDictionary {
  base::Value active_value;
  mojom::PolicySource policy_source = mojom::PolicySource::kNone;
  base::Value policy_value;
};

bool GetBoolean(const base::Value::Dict* dict,
                const char* key,
                bool value_if_key_missing_from_dict = false);

std::optional<std::string> GetString(const base::Value::Dict* dict,
                                     const char* key);

const base::Value::Dict* GetDictionary(const base::Value::Dict* dict,
                                       const char* key);

// GetManagedDictionary() returns a ManagedDictionary representing the active
// and policy values for a managed property. The types of |active_value| and
// |policy_value| are expected to match the ONC signature for the property type.
ManagedDictionary GetManagedDictionary(const base::Value::Dict* onc_dict);

mojom::ManagedStringPtr GetManagedString(const base::Value::Dict* dict,
                                         const char* key);

mojom::ManagedStringPtr GetRequiredManagedString(const base::Value::Dict* dict,
                                                 const char* key);

// Creates a Mojo Managed APN from an ONC dictionary.
mojom::ManagedApnPropertiesPtr GetManagedApnProperties(
    const base::Value::Dict* cellular_dict,
    const char* key);

// Returns true if |network_type| matches |match_type|, which may include kAll
// or kWireless.
bool NetworkTypeMatchesType(mojom::NetworkType network_type,
                            mojom::NetworkType match_type);

// Calls NetworkTypeMatchesType with |network_type| = |network|->type.
bool NetworkStateMatchesType(const mojom::NetworkStateProperties* network,
                             mojom::NetworkType type);

// Returns true if |connection_state| is in a connected state, including portal.
bool StateIsConnected(mojom::ConnectionStateType connection_state);

// Returns the signal strength for wireless network types or 0 for other types.
int GetWirelessSignalStrength(const mojom::NetworkStateProperties* network);

// Returns true if the device state InhibitReason property is set to anything
// but kNotInhibited.
bool IsInhibited(const mojom::DeviceStateProperties* device);

// Returns an ONC dictionary for network with guid |network_guid| containing a
// configuration of the network's custom APN list.
base::Value::Dict CustomApnListToOnc(const std::string& network_guid,
                                     const base::Value::List* custom_apn_list);

// Converts a list of APN types in the ONC representation to the Mojo enum
// representation.
std::vector<mojom::ApnType> OncApnTypesToMojo(
    const std::vector<std::string>& apn_types);

// Creates a Mojo APN from a ONC dictionary.
mojom::ApnPropertiesPtr GetApnProperties(const base::Value::Dict& onc_apn,
                                         bool is_apn_revamp_enabled);

// Creates a Mojo APN list from a ONC dictionary.
mojom::ManagedApnListPtr GetManagedApnList(const base::Value* value,
                                           bool is_apn_revamp_enabled);

// Converts the `cros_network_config.mojom.WiFiConfigProperties` into
// `base::Value::Dict` format which can be recognized in MojoJS. All the field
// names in the mojo interface will be transformed to JS naming style and used
// as key names.
base::Value::Dict WiFiConfigPropertiesToMojoJsValue(
    const mojo::StructPtr<
        chromeos::network_config::mojom::WiFiConfigProperties>& wifi_config);

}  // namespace chromeos::network_config

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_
