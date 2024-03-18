// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_TRANSLATION_TABLES_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_TRANSLATION_TABLES_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "chromeos/components/onc/onc_signature.h"

namespace ash::onc {

struct FieldTranslationEntry {
  const char* onc_field_name;
  const char* shill_property_name;
};

struct StringTranslationEntry {
  const char* onc_value;
  const char* shill_value;
};

// These tables contain the mapping from ONC strings to Shill strings.
// These are NULL-terminated arrays.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kNetworkTypeTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kVPNTypeTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kWiFiSecurityTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kEAPOuterTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kActivationStateTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kNetworkTechnologyTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kRoamingStateTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kOpenVpnCompressionAlgorithmTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kIKEv2AuthenticationTypeTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kApnAuthenticationTranslationTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kApnIpTypeTranslationTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kApnSourceTranslationTable[];
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const StringTranslationEntry kCheckCaptivePortalTranslationTable[];

// A separate translation table for cellular properties that are stored in a
// Shill Device instead of a Service. The |shill_property_name| entries
// reference Device properties, not Service properties.
extern const FieldTranslationEntry kCellularDeviceTable[];

// A separate translation table for IPsec properties that need to be mapped to
// an IKEv2 VPN service, while the default one for IPsec (|ipsec_fields| defined
// in the .cc file) is for the L2TP/IPsec VPN services. Only used in the
// shill-to-onc translation but not the opposite direction.
extern const FieldTranslationEntry kIPsecIKEv2Table[];

const FieldTranslationEntry* GetFieldTranslationTable(
    const chromeos::onc::OncValueSignature& onc_signature);

// Returns the translation table for EAP.Inner based on the value of EAP.Outer
// represented in Shill, or nullptr if no translation table is available for
// the given EAP.Outer value
const StringTranslationEntry* GetEapInnerTranslationTableForShillOuter(
    std::string_view shill_outer_name);

// Returns the translation table for EAP.Inner based on the value of EAP.Outer
// represented in ONC, or nullptr if no translation table is available for
// the given EAP.Outer value
const StringTranslationEntry* GetEapInnerTranslationTableForOncOuter(
    std::string_view onc_outer_value);

// Returns the path at which the translation of an ONC object will be stored in
// a Shill dictionary if its signature is |onc_signature|.
// The default is that values are stored directly in the top level of the Shill
// dictionary.
std::vector<std::string> GetPathToNestedShillDictionary(
    const chromeos::onc::OncValueSignature& onc_signature);

bool GetShillPropertyName(const std::string& onc_field_name,
                          const FieldTranslationEntry table[],
                          std::string* shill_property_name);

// Translate individual strings to Shill using the above tables.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool TranslateStringToShill(const StringTranslationEntry table[],
                            const std::string& onc_value,
                            std::string* shill_value);

// Translate individual strings to ONC using the above tables.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool TranslateStringToONC(const StringTranslationEntry table[],
                          const std::string& shill_value,
                          std::string* onc_value);

}  // namespace ash::onc

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_TRANSLATION_TABLES_H_
