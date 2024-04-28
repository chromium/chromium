// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_ONC_SIGNATURE_H_
#define CHROMEOS_COMPONENTS_ONC_ONC_SIGNATURE_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/values.h"

namespace chromeos {
namespace onc {

// Generates a default value for a field.
// This is used so that static global base::Values can be avoided (which would
// have a non-trivial destructor and are thus prohibited).
using DefaultValueSetterFunc = base::Value (*)();

struct OncValueSignature;

struct OncFieldSignature {
  const char* onc_field_name;
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const OncValueSignature* value_signature;
  // If this is non-null, it will be called if the field doesn't have a value
  // after shill->onc translation and the returned value will be assigned to the
  // field.
  DefaultValueSetterFunc default_value_setter = nullptr;
};

struct COMPONENT_EXPORT(CHROMEOS_ONC) OncValueSignature {
  base::Value::Type onc_type;
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const OncFieldSignature* fields;
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const OncValueSignature* onc_array_entry_signature;
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const OncValueSignature* base_signature;
};

COMPONENT_EXPORT(CHROMEOS_ONC)
const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name);

COMPONENT_EXPORT(CHROMEOS_ONC)
bool FieldIsCredential(const OncValueSignature& signature,
                       const std::string& onc_field_name);

COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kRecommendedSignature;
COMPONENT_EXPORT(CHROMEOS_ONC) extern const OncValueSignature kEAPSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kIssuerSubjectPatternSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCertificatePatternSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kIPsecSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kL2TPSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kXAUTHSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kOpenVPNSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kWireGuardSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kWireGuardPeerSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kWireGuardPeerListSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kThirdPartyVPNSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kARCVPNSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kVerifyX509Signature;
COMPONENT_EXPORT(CHROMEOS_ONC) extern const OncValueSignature kVPNSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kEthernetSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kTetherSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kTetherWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kIPConfigSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kSavedIPConfigSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kStaticIPConfigSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kProxyLocationSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kProxyManualSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kProxySettingsSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kWiFiSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCertificateSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kScopeSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kNetworkConfigurationSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kGlobalNetworkConfigurationSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCertificateListSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kAdminApnListSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kNetworkConfigurationListSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kToplevelConfigurationSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kEAPSubjectAlternativeNameMatchListSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kEAPSubjectAlternativeNameMatchSignature;

// Derived "ONC with State" signatures.
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kNetworkWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kWiFiWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCellularSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCellularWithStateSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCellularPaymentPortalSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCellularProviderSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCellularApnSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kCellularFoundNetworkSignature;
COMPONENT_EXPORT(CHROMEOS_ONC)
extern const OncValueSignature kSIMLockStatusSignature;

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ONC_ONC_SIGNATURE_H_
