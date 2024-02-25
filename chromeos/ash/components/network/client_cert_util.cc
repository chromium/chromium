// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/client_cert_util.h"

#include <cert.h>
#include <pk11pub.h>
#include <stddef.h>

#include <list>
#include <optional>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "components/onc/onc_constants.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::client_cert {

const char kDefaultTPMPin[] = "111111";

namespace {

// Extracts the type and descriptor (referenced GUID or client cert pattern
// or provisioning profile id) of a ONC-specified client certificate
// specification for a network
// (|dict_with_client_cert|) and stores it in |cert_config|.
void GetClientCertTypeAndDescriptor(
    onc::ONCSource onc_source,
    const base::Value::Dict& dict_with_client_cert,
    ClientCertConfig* cert_config) {
  cert_config->onc_source = onc_source;

  const std::string* identity =
      dict_with_client_cert.FindString(::onc::eap::kIdentity);
  if (identity)
    cert_config->policy_identity = *identity;

  const std::string* client_cert_type =
      dict_with_client_cert.FindString(::onc::client_cert::kClientCertType);
  if (client_cert_type)
    cert_config->client_cert_type = *client_cert_type;

  if (cert_config->client_cert_type == ::onc::client_cert::kPattern) {
    const base::Value::Dict* pattern_value =
        dict_with_client_cert.FindDict(::onc::client_cert::kClientCertPattern);
    if (pattern_value) {
      std::optional<OncCertificatePattern> pattern =
          OncCertificatePattern::ReadFromONCDictionary(*pattern_value);
      if (!pattern.has_value()) {
        LOG(ERROR) << "ClientCertPattern invalid";
        return;
      }
      cert_config->pattern = pattern.value();
    }
  } else if (cert_config->client_cert_type == ::onc::client_cert::kRef) {
    const std::string* client_cert_ref_key =
        dict_with_client_cert.FindString(::onc::client_cert::kClientCertRef);
    if (client_cert_ref_key)
      cert_config->guid = *client_cert_ref_key;
  } else if (cert_config->client_cert_type ==
             ::onc::client_cert::kProvisioningProfileId) {
    const std::string* provisioning_profile_id =
        dict_with_client_cert.FindString(
            ::onc::client_cert::kClientCertProvisioningProfileId);
    if (!provisioning_profile_id) {
      LOG(ERROR) << "ProvisioningProfileId missing";
      return;
    }
    cert_config->provisioning_profile_id = *provisioning_profile_id;
  }
}

}  // namespace

std::string GetPkcs11AndSlotIdFromEapCertId(const std::string& cert_id,
                                            int* slot_id) {
  *slot_id = -1;
  if (cert_id.empty())
    return std::string();

  size_t delimiter_pos = cert_id.find(':');
  if (delimiter_pos == std::string::npos) {
    // No delimiter found, so |cert_id| only contains the PKCS11 id.
    return cert_id;
  }
  if (delimiter_pos + 1 >= cert_id.size()) {
    LOG(ERROR) << "Empty PKCS11 id in cert id.";
    return std::string();
  }
  int parsed_slot_id;
  if (base::StringToInt(cert_id.substr(0, delimiter_pos), &parsed_slot_id))
    *slot_id = parsed_slot_id;
  else
    LOG(ERROR) << "Slot ID is not an integer. Cert ID is: " << cert_id << ".";
  return cert_id.substr(delimiter_pos + 1);
}

void GetClientCertFromShillProperties(const base::Value::Dict& shill_properties,
                                      ConfigType* cert_config_type,
                                      int* tpm_slot,
                                      std::string* pkcs11_id) {
  *cert_config_type = ConfigType::kNone;
  *tpm_slot = -1;
  pkcs11_id->clear();

  // Look for VPN specific client certificate properties.
  //
  // VPN Provider values are read from the "Provider" dictionary, not the
  // "Provider.Type", etc keys (which are used only to set the values).
  const base::Value::Dict* provider_properties =
      shill_properties.FindDict(shill::kProviderProperty);
  if (provider_properties) {
    const std::string* pkcs11_id_str = nullptr;

    // Look for OpenVPN specific properties.
    // Note: OpenVPN does not have a slot property, see crbug.com/769550.
    pkcs11_id_str =
        provider_properties->FindString(shill::kOpenVPNClientCertIdProperty);
    if (pkcs11_id_str) {
      *pkcs11_id = *pkcs11_id_str;
      *cert_config_type = ConfigType::kOpenVpn;
      return;
    }

    // Look for L2TP-IPsec specific properties.
    pkcs11_id_str =
        provider_properties->FindString(shill::kL2TPIPsecClientCertIdProperty);
    if (pkcs11_id_str) {
      *pkcs11_id = *pkcs11_id_str;

      const std::string* cert_slot = provider_properties->FindString(
          shill::kL2TPIPsecClientCertSlotProperty);
      if (cert_slot && !cert_slot->empty() &&
          !base::StringToInt(*cert_slot, tpm_slot)) {
        LOG(ERROR) << "Cert slot is not an integer: " << *cert_slot << ".";
        return;
      }

      *cert_config_type = ConfigType::kL2tpIpsec;
    }

    // Look for IKEv2 specific properties.
    pkcs11_id_str =
        provider_properties->FindString(shill::kIKEv2ClientCertIdProperty);
    if (pkcs11_id_str) {
      *pkcs11_id = *pkcs11_id_str;

      const std::string* cert_slot =
          provider_properties->FindString(shill::kIKEv2ClientCertSlotProperty);
      if (cert_slot && !cert_slot->empty() &&
          !base::StringToInt(*cert_slot, tpm_slot)) {
        LOG(ERROR) << "Cert slot is not an integer: " << *cert_slot << ".";
        return;
      }

      *cert_config_type = ConfigType::kIkev2;
    }
    return;
  }

  // Look for EAP specific client certificate properties, which can either be
  // part of a WiFi or EthernetEAP configuration.
  const std::string* cert_id =
      shill_properties.FindString(shill::kEapCertIdProperty);
  if (cert_id) {
    // Shill requires both CertID and KeyID for TLS connections, despite the
    // fact that by convention they are the same ID, because one identifies
    // the certificate and the other the private key.
    std::string key_id;
    const std::string* key_id_str =
        shill_properties.FindString(shill::kEapKeyIdProperty);
    if (key_id_str)
      key_id = *key_id_str;
    // Assume the configuration to be invalid, if the two IDs are not identical.
    if (*cert_id != key_id) {
      LOG(ERROR) << "EAP CertID differs from KeyID";
      return;
    }
    *pkcs11_id = GetPkcs11AndSlotIdFromEapCertId(*cert_id, tpm_slot);
    *cert_config_type = ConfigType::kEap;
  }
}

void SetShillProperties(const ConfigType cert_config_type,
                        const int tpm_slot,
                        const std::string& pkcs11_id,
                        base::Value::Dict& properties) {
  switch (cert_config_type) {
    case ConfigType::kNone: {
      return;
    }
    case ConfigType::kOpenVpn: {
      properties.Set(shill::kOpenVPNPinProperty, kDefaultTPMPin);
      // Note: OpenVPN does not have a slot property, see crbug.com/769550.
      properties.Set(shill::kOpenVPNClientCertIdProperty, pkcs11_id);
      break;
    }
    case ConfigType::kL2tpIpsec: {
      properties.Set(shill::kL2TPIPsecPinProperty, kDefaultTPMPin);
      properties.Set(shill::kL2TPIPsecClientCertSlotProperty,
                     base::NumberToString(tpm_slot));
      properties.Set(shill::kL2TPIPsecClientCertIdProperty, pkcs11_id);
      break;
    }
    case ConfigType::kIkev2: {
      // PIN property is not used by shill for a IKEv2 service since it is a
      // fixed value.
      properties.Set(shill::kIKEv2ClientCertSlotProperty,
                     base::NumberToString(tpm_slot));
      properties.Set(shill::kIKEv2ClientCertIdProperty, pkcs11_id);
      break;
    }
    case ConfigType::kEap: {
      properties.Set(shill::kEapPinProperty, kDefaultTPMPin);
      std::string key_id =
          base::StringPrintf("%i:%s", tpm_slot, pkcs11_id.c_str());

      // Shill requires both CertID and KeyID for TLS connections, despite the
      // fact that by convention they are the same ID, because one identifies
      // the certificate and the other the private key.
      properties.Set(shill::kEapCertIdProperty, key_id);
      properties.Set(shill::kEapKeyIdProperty, key_id);
      break;
    }
  }
}

void SetEmptyShillProperties(const ConfigType cert_config_type,
                             base::Value::Dict& properties) {
  switch (cert_config_type) {
    case ConfigType::kNone: {
      return;
    }
    case ConfigType::kOpenVpn: {
      properties.Set(shill::kOpenVPNPinProperty, std::string());
      properties.Set(shill::kOpenVPNClientCertIdProperty, std::string());
      break;
    }
    case ConfigType::kL2tpIpsec: {
      properties.Set(shill::kL2TPIPsecPinProperty, std::string());
      properties.Set(shill::kL2TPIPsecClientCertSlotProperty, std::string());
      properties.Set(shill::kL2TPIPsecClientCertIdProperty, std::string());
      break;
    }
    case ConfigType::kIkev2: {
      // PIN property is not used by shill for a IKEv2 service since it is a
      // fixed value.
      properties.Set(shill::kIKEv2ClientCertSlotProperty, std::string());
      properties.Set(shill::kIKEv2ClientCertIdProperty, std::string());
      break;
    }
    case ConfigType::kEap: {
      properties.Set(shill::kEapPinProperty, std::string());
      // Shill requires both CertID and KeyID for TLS connections, despite the
      // fact that by convention they are the same ID, because one identifies
      // the certificate and the other the private key.
      properties.Set(shill::kEapCertIdProperty, std::string());
      properties.Set(shill::kEapKeyIdProperty, std::string());
      break;
    }
  }
}

ClientCertConfig::ClientCertConfig()
    : location(ConfigType::kNone),
      client_cert_type(onc::client_cert::kClientCertTypeNone) {}

ClientCertConfig::ClientCertConfig(const ClientCertConfig& other) = default;

ClientCertConfig::~ClientCertConfig() = default;

ResolvedCert::ResolvedCert(
    Status status,
    int slot_id,
    const std::string& pkcs11_id,
    base::flat_map<std::string, std::string> variable_expansions)
    : status_(status),
      slot_id_(slot_id),
      pkcs11_id_(pkcs11_id),
      variable_expansions_(variable_expansions) {}

ResolvedCert::~ResolvedCert() = default;

ResolvedCert::ResolvedCert(ResolvedCert&& other) = default;

ResolvedCert& ResolvedCert::operator=(ResolvedCert&& other) = default;

// static
ResolvedCert ResolvedCert::NotKnownYet() {
  return ResolvedCert(Status::kNotKnownYet, -1, std::string(), {});
}

// static
ResolvedCert ResolvedCert::NothingMatched() {
  return ResolvedCert(Status::kNothingMatched, -1, std::string(), {});
}

// static
ResolvedCert ResolvedCert::CertMatched(
    int slot_id,
    const std::string& pkcs11_id,
    base::flat_map<std::string, std::string> variable_expansions) {
  return ResolvedCert(Status::kCertMatched, slot_id, pkcs11_id,
                      std::move(variable_expansions));
}

ResolvedCert::Status ResolvedCert::status() const {
  return status_;
}

int ResolvedCert::slot_id() const {
  DCHECK_EQ(status(), Status::kCertMatched);
  return slot_id_;
}

const std::string& ResolvedCert::pkcs11_id() const {
  DCHECK_EQ(status(), Status::kCertMatched);
  return pkcs11_id_;
}

const base::flat_map<std::string, std::string>&
ResolvedCert::variable_expansions() const {
  DCHECK_EQ(status(), Status::kCertMatched);
  return variable_expansions_;
}

bool operator==(const ResolvedCert& lhs, const ResolvedCert& rhs) {
  if (lhs.status() != rhs.status())
    return false;

  if (lhs.status() == ResolvedCert::Status::kCertMatched) {
    // Compare attributes of the matched certificate.
    return lhs.slot_id() == rhs.slot_id() &&
           lhs.pkcs11_id() == rhs.pkcs11_id() &&
           lhs.variable_expansions() == rhs.variable_expansions();
  }

  return true;
}

// Uses a template type to easily implement a const and a non-const version.
template <typename DictType>
DictType* GetOncClientCertConfigDict(DictType& network_config,
                                     ConfigType* out_config_type) {
  DictType* wifi = network_config.FindDict(::onc::network_config::kWiFi);
  if (wifi) {
    DictType* eap = wifi->FindDict(::onc::wifi::kEAP);
    if (!eap)
      return nullptr;
    if (out_config_type)
      *out_config_type = ConfigType::kEap;
    return eap;
  }

  DictType* ethernet =
      network_config.FindDict(::onc::network_config::kEthernet);
  if (ethernet) {
    DictType* eap = ethernet->FindDict(::onc::wifi::kEAP);
    if (!eap)
      return nullptr;
    if (out_config_type)
      *out_config_type = ConfigType::kEap;
    return eap;
  }

  DictType* vpn = network_config.FindDict(::onc::network_config::kVPN);
  if (vpn) {
    DictType* openvpn = vpn->FindDict(::onc::vpn::kOpenVPN);
    DictType* ipsec = vpn->FindDict(::onc::vpn::kIPsec);
    DictType* l2tp = vpn->FindDict(::onc::vpn::kL2TP);
    if (openvpn) {
      if (out_config_type)
        *out_config_type = ConfigType::kOpenVpn;
      return openvpn;
    }
    if (ipsec) {
      // Currently we support two kinds of IPsec-based VPN:
      // - L2TP/IPsec: IKE version is 1 and |l2tp| is set;
      // - IKEv2: IKE version is 2 and |l2tp| is not set.
      // Thus we only use |l2tp| to distinguish between these two cases.
      if (out_config_type)
        *out_config_type = l2tp ? ConfigType::kL2tpIpsec : ConfigType::kIkev2;
      return ipsec;
    }
  }

  return nullptr;
}

void OncToClientCertConfig(::onc::ONCSource onc_source,
                           const base::Value::Dict& network_config,
                           ClientCertConfig* cert_config) {
  *cert_config = ClientCertConfig();

  const base::Value::Dict* dict_with_client_cert =
      GetOncClientCertConfigDict(network_config, &(cert_config->location));
  if (dict_with_client_cert) {
    GetClientCertTypeAndDescriptor(onc_source, *dict_with_client_cert,
                                   cert_config);
  }
}

void SetResolvedCertInOnc(const ResolvedCert& resolved_cert,
                          base::Value::Dict& network_config) {
  if (resolved_cert.status() == ResolvedCert::Status::kNotKnownYet)
    return;

  base::Value::Dict* dict_with_client_cert =
      GetOncClientCertConfigDict(network_config, /*out_config_type=*/nullptr);
  if (!dict_with_client_cert)
    return;
  dict_with_client_cert->Set(::onc::client_cert::kClientCertType,
                             ::onc::client_cert::kPKCS11Id);
  if (resolved_cert.status() == ResolvedCert::Status::kNothingMatched) {
    // Empty PKCS11Id means that no certificate has been selected and it
    // should be cleared in shill.
    dict_with_client_cert->Set(::onc::client_cert::kClientCertPKCS11Id,
                               std::string());
  } else {
    dict_with_client_cert->Set(
        ::onc::client_cert::kClientCertPKCS11Id,
        base::StringPrintf("%i:%s", resolved_cert.slot_id(),
                           resolved_cert.pkcs11_id().c_str()));
  }
  dict_with_client_cert->Remove(::onc::client_cert::kClientCertPattern);
}

}  // namespace ash::client_cert
