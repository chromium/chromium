// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_NETWORK_ONC_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_NETWORK_ONC_UTILS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "net/cert/scoped_nss_types.h"

class PrefService;

namespace chromeos::onc {
struct OncValueSignature;
}

namespace user_manager {
class User;
}

namespace ash {

class NetworkState;

namespace onc {

// Returns a network type pattern for matching the ONC type string.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
NetworkTypePattern NetworkTypePatternFromOncType(const std::string& type);

// Translates |onc_proxy_settings|, which must be a valid ONC ProxySettings
// dictionary, to a ProxyConfig dictionary (see proxy_config_dictionary.h).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::optional<base::Value::Dict> ConvertOncProxySettingsToProxyConfig(
    const base::Value::Dict& onc_proxy_settings);

// Translates |proxy_config_dict|, which must be a valid ProxyConfig dictionary
// (see proxy_config_dictionary.h) to an ONC ProxySettings dictionary.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::optional<base::Value::Dict> ConvertProxyConfigToOncProxySettings(
    const base::Value::Dict& proxy_config_dict);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::flat_map<std::string, std::string> GetVariableExpansionsForUser(
    const user_manager::User* user);

// Returns the number of networks successfully imported.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
int ImportNetworksForUser(const user_manager::User* user,
                          const base::Value::List& network_configs,
                          std::string* error);

// Convenience function to retrieve the "AllowOnlyPolicyNetworksToAutoconnect"
// setting from the global network configuration (see
// GetGlobalConfigFromPolicy).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool PolicyAllowsOnlyPolicyNetworksToAutoconnect(bool for_active_user);

// Returns the effective (user or device) policy for network |network|. Both
// |profile_prefs| and |local_state_prefs| might be NULL. Returns NULL if no
// applicable policy is found. Sets |onc_source| accordingly.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
const base::Value::Dict* GetPolicyForNetwork(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs,
    const NetworkState& network,
    ::onc::ONCSource* onc_source);

// Convenience function to check only whether a policy for a network exists.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool HasPolicyForNetwork(const PrefService* profile_prefs,
                         const PrefService* local_state_prefs,
                         const NetworkState& network);

// Checks whether a WiFi dictionary object has the ${PASSWORD} substitution
// variable set as the password.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool HasUserPasswordSubstitutionVariable(
    const chromeos::onc::OncValueSignature& signature,
    const base::Value::Dict& onc_object);

// Checks whether a list of network objects has at least one network with the
// ${PASSWORD} substitution variable set as the password.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool HasUserPasswordSubstitutionVariable(
    const base::Value::List& network_configs);

}  // namespace onc
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_NETWORK_ONC_UTILS_H_
