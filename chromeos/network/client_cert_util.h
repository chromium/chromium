// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
#define CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/certificate_pattern.h"
#include "components/onc/onc_constants.h"

namespace base {
class Value;
class DictionaryValue;
}

namespace net {
struct CertPrincipal;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}

namespace chromeos {

namespace client_cert {

CHROMEOS_EXPORT extern const char kDefaultTPMPin[];

enum ConfigType {
  CONFIG_TYPE_NONE,
  CONFIG_TYPE_OPENVPN,
  CONFIG_TYPE_IPSEC,
  CONFIG_TYPE_EAP
};

struct CHROMEOS_EXPORT ClientCertConfig {
  ClientCertConfig();
  ClientCertConfig(const ClientCertConfig& other);
  ~ClientCertConfig();

  // Independent of whether the client cert (pattern or reference) is
  // configured, the location determines whether this network configuration
  // supports client certs and what kind of configuration it requires.
  ConfigType location;

  // One of the ClientCertTypes defined in ONC: |kNone|, |kRef|, or |kPattern|.
  std::string client_cert_type;

  // If |client_cert_type| equals |kPattern|, this contains the pattern.
  CertificatePattern pattern;

  // If |client_cert_type| equals |kRef|, this contains the GUID of the
  // referenced certificate.
  std::string guid;

  // The value of |kIdentity|, to enable substitutions.
  std::string policy_identity;

  // source of this ClientCertConfig.
  ::onc::ONCSource onc_source;
};

// Returns true only if any fields set in this pattern match exactly with
// similar fields in the principal.  If organization_ or organizational_unit_
// are set, then at least one of the organizations or units in the principal
// must match.
bool CertPrincipalMatches(const IssuerSubjectPattern& pattern,
                          const net::CertPrincipal& principal);

// Returns the PKCS11 and slot ID of |cert_id|, which is expected to be a
// value of the Shill property |kEapCertIdProperty| or |kEapKeyIdProperty|,
// either of format "<pkcs11_id>" or "<slot_id>:<pkcs11_id>".
CHROMEOS_EXPORT std::string GetPkcs11AndSlotIdFromEapCertId(
    const std::string& cert_id,
    int* slot_id);

// Reads the client certificate configuration from the Shill Service properties
// |shill_properties|.
// If such a configuration is found, the values |cert_config_type|, |tpm_slot|
// and |pkcs11_id| are filled accordingly. In case of OpenVPN or because the
// property was not set, |tpm_slot| will be set to -1.
// If an error occurred or no client configuration is found, |cert_config_type|
// will be set to CONFIG_TYPE_NONE, |tpm_slot| to -1 and |pkcs11_id| to the
// empty string.
CHROMEOS_EXPORT void GetClientCertFromShillProperties(
    const base::DictionaryValue& shill_properties,
    ConfigType* cert_config_type,
    int* tpm_slot,
    std::string* pkcs11_id);

// Sets the properties of a client cert and the TPM slot that it's contained in.
// |cert_config_type| determines which dictionary entries to set.
CHROMEOS_EXPORT void SetShillProperties(const ConfigType cert_config_type,
                                        const int tpm_slot,
                                        const std::string& pkcs11_id,
                                        base::Value* properties);

// Like SetShillProperties but instead sets the properties to empty strings.
// This should be used to clear previously set client certificate properties.
CHROMEOS_EXPORT void SetEmptyShillProperties(const ConfigType cert_config_type,
                                             base::Value* properties);

// Determines the type of the CertificatePattern configuration, i.e. is it a
// pattern within an EAP, IPsec or OpenVPN configuration.
CHROMEOS_EXPORT void OncToClientCertConfig(
    ::onc::ONCSource onc_source,
    const base::DictionaryValue& network_config,
    ClientCertConfig* cert_config);

}  // namespace client_cert

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
