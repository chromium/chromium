// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_UTILS_H_
#define CHROMEOS_NETWORK_ONC_ONC_UTILS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "net/cert/scoped_nss_types.h"

class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_manager {
class User;
}

namespace chromeos {

class NetworkState;

namespace onc {

struct OncValueSignature;

// A valid but empty (no networks and no certificates) and unencrypted
// configuration.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const char kEmptyUnencryptedConfiguration[];

typedef std::map<std::string, std::string> CertPEMsByGUIDMap;

// Parses |json| according to the JSON format. If |json| is a JSON formatted
// dictionary, the function returns the dictionary value, otherwise returns
// an empty Value.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value ReadDictionaryFromJson(const std::string& json);

// Decrypts the given EncryptedConfiguration |onc| (see the ONC specification)
// using |passphrase|. The resulting UnencryptedConfiguration is returned. If an
// error occurs, returns an empty Value.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value Decrypt(const std::string& passphrase, const base::Value& onc);

// For logging only: strings not user facing.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetSourceAsString(::onc::ONCSource source);

// Replaces all expandable fields that are mentioned in the ONC
// specification. The object of |onc_object| is modified in place.
// The substitution is performed using the passed |variable_expander|, which
// defines the placeholder-value mapping.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ExpandStringsInOncObject(const OncValueSignature& signature,
                              const VariableExpander& variable_expander,
                              base::DictionaryValue* onc_object);

// Replaces expandable fields in the networks of |network_configs|, which must
// be a list of ONC NetworkConfigurations. See ExpandStringsInOncObject above.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ExpandStringsInNetworks(const VariableExpander& variable_expander,
                             base::ListValue* network_configs);

// Fills in all missing HexSSID fields that are mentioned in the ONC
// specification. The object of |onc_object| is modified in place.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void FillInHexSSIDFieldsInOncObject(const OncValueSignature& signature,
                                    base::Value* onc_object);

// If the SSID field is set, but HexSSID is not, converts the contents of the
// SSID field to UTF-8 encoding, creates the hex representation and assigns the
// result to HexSSID.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void FillInHexSSIDField(base::Value* wifi_fields);

// Creates a copy of |onc_object| with all values of sensitive fields replaced
// by |mask|. To find sensitive fields, signature and field name are checked
// with the function FieldIsCredential().
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value MaskCredentialsInOncObject(const OncValueSignature& signature,
                                       const base::Value& onc_object,
                                       const std::string& mask);

// Decrypts |onc_blob| with |passphrase| if necessary. Clears |network_configs|,
// |global_network_config| and |certificates| and fills them with the validated
// NetworkConfigurations, GlobalNetworkConfiguration and Certificates of
// |onc_blob|. Callers can pass nullptr as any of |network_configs|,
// |global_network_config|, |certificates| if they're not interested in the
// respective values. Returns false if any validation errors or warnings
// occurred in any segments (i.e. not only those requested by the caller). Even
// if false is returned, some configuration might be added to the output
// arguments and should be further processed by the caller.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool ParseAndValidateOncForImport(const std::string& onc_blob,
                                  ::onc::ONCSource onc_source,
                                  const std::string& passphrase,
                                  base::ListValue* network_configs,
                                  base::DictionaryValue* global_network_config,
                                  base::ListValue* certificates);

// Parse the given PEM encoded certificate |pem_encoded| and return the
// contained DER encoding. Returns an empty string on failure.
std::string DecodePEM(const std::string& pem_encoded);

// Parse the given PEM encoded certificate |pem_encoded| and create a
// CERTCertificate from it.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
net::ScopedCERTCertificate DecodePEMCertificate(const std::string& pem_encoded);

// Replaces all references by GUID to Server or CA certs by their PEM
// encoding. Returns true if all references could be resolved. Otherwise returns
// false and network configurations with unresolveable references are removed
// from |network_configs|. |network_configs| must be a list of ONC
// NetworkConfiguration dictionaries.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool ResolveServerCertRefsInNetworks(const CertPEMsByGUIDMap& certs_by_guid,
                                     base::ListValue* network_configs);

// Replaces all references by GUID to Server or CA certs by their PEM
// encoding. Returns true if all references could be resolved. |network_config|
// must be a ONC NetworkConfiguration.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool ResolveServerCertRefsInNetwork(const CertPEMsByGUIDMap& certs_by_guid,
                                    base::DictionaryValue* network_config);

// Returns a network type pattern for matching the ONC type string.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
NetworkTypePattern NetworkTypePatternFromOncType(const std::string& type);

// Translates |onc_proxy_settings|, which must be a valid ONC ProxySettings
// dictionary, to a ProxyConfig dictionary (see proxy_config_dictionary.h).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value ConvertOncProxySettingsToProxyConfig(
    const base::Value& onc_proxy_settings);

// Translates |proxy_config_value|, which must be a valid ProxyConfig dictionary
// (see proxy_config_dictionary.h) to an ONC ProxySettings dictionary.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value ConvertProxyConfigToOncProxySettings(
    const base::Value& proxy_config_value);

// Replaces user-specific string placeholders in |network_configs|, which must
// be a list of ONC NetworkConfigurations. Currently only user name placeholders
// are implemented, which are replaced by attributes from |user|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ExpandStringPlaceholdersInNetworksForUser(
    const user_manager::User* user,
    base::ListValue* network_configs);

// Returns the number of networks successfully imported.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
int ImportNetworksForUser(const user_manager::User* user,
                          const base::ListValue& network_configs,
                          std::string* error);

// Looks up the policy for |guid| for the current active user and sets
// |global_config| (if not NULL) and |onc_source| (if not NULL) accordingly. If
// |guid| is empty, returns NULL and sets the |global_config| and |onc_source|
// if a policy is found.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
const base::DictionaryValue* FindPolicyForActiveUser(
    const std::string& guid,
    ::onc::ONCSource* onc_source);

// Convenvience function to retrieve the "AllowOnlyPolicyNetworksToAutoconnect"
// setting from the global network configuration (see
// GetGlobalConfigFromPolicy).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool PolicyAllowsOnlyPolicyNetworksToAutoconnect(bool for_active_user);

// Returns the effective (user or device) policy for network |network|. Both
// |profile_prefs| and |local_state_prefs| might be NULL. Returns NULL if no
// applicable policy is found. Sets |onc_source| accordingly.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
const base::DictionaryValue* GetPolicyForNetwork(
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
bool HasUserPasswordSubsitutionVariable(const OncValueSignature& signature,
                                        base::DictionaryValue* onc_object);

// Checks whether a list of network objects has at least one network with the
// ${PASSWORD} substitution variable set as the password.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool HasUserPasswordSubsitutionVariable(base::ListValue* network_configs);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_UTILS_H_
