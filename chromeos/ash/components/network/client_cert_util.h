// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CLIENT_CERT_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CLIENT_CERT_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "chromeos/ash/components/network/onc/onc_certificate_pattern.h"
#include "components/onc/onc_constants.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class Value;
}

namespace ash::client_cert {

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

// Identifies a resolved client certificate (e.g. after matching existing client
// certificates against an ONC ClientCertPattern).
// Can also signify that no certificate has been resolved yet - see the Status
// enum.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ResolvedCert {
 public:
  ~ResolvedCert();
  ResolvedCert(ResolvedCert&& other);
  ResolvedCert& operator=(ResolvedCert&& other);

  static ResolvedCert NotKnownYet();
  static ResolvedCert NothingMatched();
  static ResolvedCert CertMatched(
      int slot_id,
      const std::string& pkcs11_id,
      base::flat_map<std::string, std::string> variable_expansions);

  enum class Status {
    // It is not known yet if a matching client certificate exists.
    kNotKnownYet,
    // No matching client certificate has been found.
    kNothingMatched,
    // A matching client certificate has been found.
    kCertMatched
  };

  Status status() const;

  // The PKCS#11 slot id the resolved certificate is stored on.
  // Only callable if `status()` is `Status::kCertMatched`.
  int slot_id() const;
  // The PKCS#11 object id of the resolved certificiate.
  // Only callable if `status()` is `Status::kCertMatched`.
  const std::string& pkcs11_id() const;
  // ONC Variable expansions extracted from the resolved certificate.
  // Only callable if `status()` is `Status::kCertMatched`.
  const base::flat_map<std::string, std::string>& variable_expansions() const;

 private:
  ResolvedCert(Status status,
               int slot_id,
               const std::string& pkcs11_id,
               base::flat_map<std::string, std::string> variable_expansions);
  // The status of certificate resolution.
  Status status_;

  // The PKCS11 slot on which the certificate is stored.
  // Only relevant if `status` is `Status::kCertMatched`.
  int slot_id_;
  // The PKCS11 object id of the certificate and private key.
  // Only relevant if `status` is `Status::kCertMatched`.
  std::string pkcs11_id_;
  // ONC Variable expansions generated from the certificate's contents.
  // Only relevant if `status` is `Status::kCertMatched`.
  base::flat_map<std::string, std::string> variable_expansions_;
};

COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool operator==(const ResolvedCert& lhs, const ResolvedCert& rhs);

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
void GetClientCertFromShillProperties(const base::Value::Dict& shill_properties,
                                      ConfigType* cert_config_type,
                                      int* tpm_slot,
                                      std::string* pkcs11_id);

// Sets the properties of a client cert and the TPM slot that it's contained in.
// |cert_config_type| determines which dictionary entries to set.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetShillProperties(const ConfigType cert_config_type,
                        const int tpm_slot,
                        const std::string& pkcs11_id,
                        base::Value::Dict& properties);

// Like SetShillProperties but instead sets the properties to empty strings.
// This should be used to clear previously set client certificate properties.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetEmptyShillProperties(const ConfigType cert_config_type,
                             base::Value::Dict& properties);

// Determines the type of the OncCertificatePattern configuration, i.e. is it a
// pattern within an EAP, IPsec or OpenVPN configuration.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void OncToClientCertConfig(::onc::ONCSource onc_source,
                           const base::Value::Dict& network_config,
                           ClientCertConfig* cert_config);

// Sets the client certificate described by `network_config` as the selected
// client certificate of `network_config`.
// If `resolved_cert` has `Status::kNotKnownYet`, will not modify
// `network_config`. If `resolved_cert` is `Status::kNoCert`, will set an empty
// PKCS11Id. Otherwise will configure `network_config` to use the PKCS11Id from
// `resolved_cert`.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetResolvedCertInOnc(const ResolvedCert& resolved_cert,
                          base::Value::Dict& network_config);

}  // namespace ash::client_cert

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CLIENT_CERT_UTIL_H_
