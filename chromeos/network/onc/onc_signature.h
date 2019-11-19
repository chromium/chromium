// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_SIGNATURE_H_
#define CHROMEOS_NETWORK_ONC_ONC_SIGNATURE_H_

#include <string>

#include "base/component_export.h"
#include "base/values.h"

namespace chromeos {
namespace onc {

struct OncValueSignature;

struct OncFieldSignature {
  const char* onc_field_name;
  const OncValueSignature* value_signature;
};

struct COMPONENT_EXPORT(CHROMEOS_NETWORK) OncValueSignature {
  base::Value::Type onc_type;
  const OncFieldSignature* fields;
  const OncValueSignature* onc_array_entry_signature;
  const OncValueSignature* base_signature;
};

COMPONENT_EXPORT(CHROMEOS_NETWORK)
const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool FieldIsCredential(const OncValueSignature& signature,
                       const std::string& onc_field_name);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kRecommendedSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const OncValueSignature kEAPSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kIssuerSubjectPatternSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCertificatePatternSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kIPsecSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kL2TPSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kXAUTHSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kOpenVPNSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kThirdPartyVPNSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kARCVPNSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kVerifyX509Signature;
COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const OncValueSignature kVPNSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kEthernetSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kTetherSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kTetherWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kIPConfigSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kSavedIPConfigSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kStaticIPConfigSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kProxyLocationSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kProxyManualSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kProxySettingsSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kWiFiSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCertificateSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kScopeSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kNetworkConfigurationSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kGlobalNetworkConfigurationSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCertificateListSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kNetworkConfigurationListSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kToplevelConfigurationSignature;

// Derived "ONC with State" signatures.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kNetworkWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kWiFiWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCellularSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCellularWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCellularPaymentPortalSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCellularProviderSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCellularApnSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kCellularFoundNetworkSignature;
COMPONENT_EXPORT(CHROMEOS_NETWORK)
extern const OncValueSignature kSIMLockStatusSignature;

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_SIGNATURE_H_
