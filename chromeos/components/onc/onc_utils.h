// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_ONC_UTILS_H_
#define CHROMEOS_COMPONENTS_ONC_ONC_UTILS_H_

#include <map>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace chromeos {

class VariableExpander;

namespace onc {

struct OncValueSignature;

using CertPEMsByGUIDMap = std::map<std::string, std::string>;

// Parses |json| according to the JSON format. If |json| is a JSON formatted
// dictionary, the function populates |dict| and returns true, otherwise returns
// false and |dict| is unchanged.
COMPONENT_EXPORT(CHROMEOS_ONC)
std::optional<base::Value::Dict> ReadDictionaryFromJson(
    const std::string& json);

// Decrypts the given EncryptedConfiguration |onc| (see the ONC specification)
// using |passphrase|. The resulting UnencryptedConfiguration is returned. If an
// error occurs, returns nullopt.
COMPONENT_EXPORT(CHROMEOS_ONC)
std::optional<base::Value::Dict> Decrypt(const std::string& passphrase,
                                         const base::Value::Dict& onc);

// For logging only: strings not user facing.
COMPONENT_EXPORT(CHROMEOS_ONC)
std::string GetSourceAsString(::onc::ONCSource source);

// Replaces all expandable fields that are mentioned in the ONC
// specification. The object of |onc_object| is modified in place.
// The substitution is performed using the passed |variable_expander|, which
// defines the placeholder-value mapping.
COMPONENT_EXPORT(CHROMEOS_ONC)
void ExpandStringsInOncObject(const OncValueSignature& signature,
                              const VariableExpander& variable_expander,
                              base::Value::Dict* onc_object);

// Replaces expandable fields in the networks of |network_configs|, which must
// be a list of ONC NetworkConfigurations. See ExpandStringsInOncObject above.
COMPONENT_EXPORT(CHROMEOS_ONC)
void ExpandStringsInNetworks(const VariableExpander& variable_expander,
                             base::Value::List& network_configs);

// Fills in all missing CustomAPNList fields that are mentioned in the
// ONC specification with the value of |custom_apn_list|. The object of
// |onc_object| is modified in place.
COMPONENT_EXPORT(CHROMEOS_ONC)
void FillInCellularCustomAPNListFieldsInOncObject(
    const OncValueSignature& signature,
    base::Value::Dict& onc_object,
    const base::Value::List* custom_apn_list);

// Fills in all missing HexSSID fields that are mentioned in the ONC
// specification. The object of |onc_object| is modified in place.
COMPONENT_EXPORT(CHROMEOS_ONC)
void FillInHexSSIDFieldsInOncObject(const OncValueSignature& signature,
                                    base::Value::Dict& onc_object);

// If the SSID field is set, but HexSSID is not, converts the contents of the
// SSID field to UTF-8 encoding, creates the hex representation and assigns the
// result to HexSSID.
COMPONENT_EXPORT(CHROMEOS_ONC)
void FillInHexSSIDField(base::Value::Dict& wifi_fields);

// Sets missing HiddenSSID fields to default value that is specified in the ONC
// specification. The object of |onc_object| is modified in place.
COMPONENT_EXPORT(CHROMEOS_ONC)
void SetHiddenSSIDFieldInOncObject(const OncValueSignature& signature,
                                   base::Value::Dict& onc_object);

// If the HiddenSSID field is not set, sets it to default value(false). If the
// HiddenSSID field is set already, does nothing.
COMPONENT_EXPORT(CHROMEOS_ONC)
void SetHiddenSSIDField(base::Value::Dict& wifi_fields);

// Creates a copy of |onc_object| with all values of sensitive fields replaced
// by |mask|. To find sensitive fields, signature and field name are checked
// with the function FieldIsCredential().
COMPONENT_EXPORT(CHROMEOS_ONC)
base::Value::Dict MaskCredentialsInOncObject(
    const OncValueSignature& signature,
    const base::Value::Dict& onc_object,
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
COMPONENT_EXPORT(CHROMEOS_ONC)
bool ParseAndValidateOncForImport(const std::string& onc_blob,
                                  ::onc::ONCSource onc_source,
                                  const std::string& passphrase,
                                  base::Value::List* network_configs,
                                  base::Value::Dict* global_network_config,
                                  base::Value::List* certificates);

// Parse the given PEM encoded certificate |pem_encoded| and return the
// contained DER encoding. Returns an empty string on failure.
std::string DecodePEM(const std::string& pem_encoded);

// Replaces all references by GUID to Server or CA certs by their PEM
// encoding. Returns true if all references could be resolved. Otherwise returns
// false and network configurations with unresolvable references are removed
// from |network_configs|. |network_configs| must be a list of ONC
// NetworkConfiguration dictionaries.
COMPONENT_EXPORT(CHROMEOS_ONC)
bool ResolveServerCertRefsInNetworks(const CertPEMsByGUIDMap& certs_by_guid,
                                     base::Value::List& network_configs);

// Replaces all references by GUID to Server or CA certs by their PEM
// encoding. Returns true if all references could be resolved. |network_config|
// must be a ONC NetworkConfiguration.
COMPONENT_EXPORT(CHROMEOS_ONC)
bool ResolveServerCertRefsInNetwork(const CertPEMsByGUIDMap& certs_by_guid,
                                    base::Value::Dict& network_config);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ONC_ONC_UTILS_H_
