// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
#define CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "chromeos/network/onc/onc_certificate_pattern.h"
#include "components/onc/onc_constants.h"

namespace base {
class Value;
}

namespace chromeos {

namespace client_cert {

COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDefaultTPMPin[];

enum class ConfigType {
  kNone,
  kOpenVpn,
  // We need two separate types for L2TP/IPsec and IKEv2: both of them are used
  // for IPsec and have the same properties, the only difference is that they
  // are mapped to different sets of shill service properties.
  kL2tpIpsec,
  kIkev2,
  kEap,
};

struct COMPONENT_EXPORT(CHROMEOS_NETWORK) ClientCertConfig {
  ClientCertConfig();
  ClientCertConfig(const ClientCertConfig& other);
  ~ClientCertConfig();

  // Independent of whether the client cert (pattern, reference, or provisioning
  // profile id) is configured, the location determines whether this network
  // configuration supports client certs and what kind of configuration it
  // requires.
  ConfigType location;

  // One of the ClientCertTypes defined in ONC: |kNone|, |kRef|,
  // |kProvisioningProfileId|, or |kPattern|.
  std::string client_cert_type;

  // If |client_cert_type| equals |kPattern|, this contains the pattern.
  OncCertificatePattern pattern;

  // If |client_cert_type| equals |kRef|, this contains the GUID of the
  // referenced certificate.
  std::string guid;

  // If |client_cert_type| equals |kProvisioningProfileId|, this contains the id
  // of the referenced certificate.
  std::string provisioning_profile_id;

  // The value of |kIdentity|, to enable substitutions.
  std::string policy_identity;

  // source of this ClientCertConfig.
  ::onc::ONCSource onc_source;
};

// Returns the PKCS11 and slot ID of |cert_id|, which is expected to be a
// value of the Shill property |kEapCertIdProperty| or |kEapKeyIdProperty|,
// either of format "<pkcs11_id>" or "<slot_id>:<pkcs11_id>".
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetPkcs11AndSlotIdFromEapCertId(const std::string& cert_id,
                                            int* slot_id);

// Reads the client certificate configuration from the Shill Service properties
// |shill_properties|.
// If such a configuration is found, the values |cert_config_type|, |tpm_slot|
// and |pkcs11_id| are filled accordingly. In case of OpenVPN or because the
// property was not set, |tpm_slot| will be set to -1.
// If an error occurred or no client configuration is found, |cert_config_type|
// will be set to ConfigType::kNone, |tpm_slot| to -1 and |pkcs11_id| to the
// empty string.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void GetClientCertFromShillProperties(const base::Value& shill_properties,
                                      ConfigType* cert_config_type,
                                      int* tpm_slot,
                                      std::string* pkcs11_id);

// Sets the properties of a client cert and the TPM slot that it's contained in.
// |cert_config_type| determines which dictionary entries to set.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetShillProperties(const ConfigType cert_config_type,
                        const int tpm_slot,
                        const std::string& pkcs11_id,
                        base::Value* properties);

// Like SetShillProperties but instead sets the properties to empty strings.
// This should be used to clear previously set client certificate properties.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetEmptyShillProperties(const ConfigType cert_config_type,
                             base::Value* properties);

// Determines the type of the OncCertificatePattern configuration, i.e. is it a
// pattern within an EAP, IPsec or OpenVPN configuration.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void OncToClientCertConfig(::onc::ONCSource onc_source,
                           const base::Value& network_config,
                           ClientCertConfig* cert_config);

}  // namespace client_cert

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
