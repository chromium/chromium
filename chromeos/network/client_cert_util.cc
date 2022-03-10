// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/client_cert_util.h"

#include <cert.h>
#include <pk11pub.h>
#include <stddef.h>

#include <list>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "components/onc/onc_constants.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace client_cert {

const char kDefaultTPMPin[] = "111111";

namespace {

// Extracts the type and descriptor (referenced GUID or client cert pattern
// or provisioning profile id) of a ONC-specified client certificate
// specification for a network
// (|dict_with_client_cert|) and stores it in |cert_config|.
void GetClientCertTypeAndDescriptor(onc::ONCSource onc_source,
                                    const base::Value& dict_with_client_cert,
                                    ClientCertConfig* cert_config) {
  cert_config->onc_source = onc_source;

  const std::string* identity =
      dict_with_client_cert.FindStringKey(::onc::eap::kIdentity);
  if (identity)
    cert_config->policy_identity = *identity;

  const std::string* client_cert_type =
      dict_with_client_cert.FindStringKey(::onc::client_cert::kClientCertType);
  if (client_cert_type)
    cert_config->client_cert_type = *client_cert_type;

  if (cert_config->client_cert_type == ::onc::client_cert::kPattern) {
    const base::Value* pattern_value = dict_with_client_cert.FindKeyOfType(
        ::onc::client_cert::kClientCertPattern, base::Value::Type::DICTIONARY);
    if (pattern_value) {
      absl::optional<OncCertificatePattern> pattern =
          OncCertificatePattern::ReadFromONCDictionary(*pattern_value);
      if (!pattern.has_value()) {
        LOG(ERROR) << "ClientCertPattern invalid";
        return;
      }
      cert_config->pattern = pattern.value();
    }
  } else if (cert_config->client_cert_type == ::onc::client_cert::kRef) {
    const base::Value* client_cert_ref_key =
        dict_with_client_cert.FindKeyOfType(::onc::client_cert::kClientCertRef,
                                            base::Value::Type::STRING);
    if (client_cert_ref_key)
      cert_config->guid = client_cert_ref_key->GetString();
  } else if (cert_config->client_cert_type ==
             ::onc::client_cert::kProvisioningProfileId) {
    const std::string* provisioning_profile_id =
        dict_with_client_cert.FindStringKey(
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

void GetClientCertFromShillProperties(const base::Value& shill_properties,
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
  const base::Value* provider_properties =
      shill_properties.FindDictKey(shill::kProviderProperty);
  if (provider_properties) {
    const std::string* pkcs11_id_str = nullptr;

    // Look for OpenVPN specific properties.
    pkcs11_id_str =
        provider_properties->FindStringKey(shill::kOpenVPNClientCertIdProperty);
    if (pkcs11_id_str) {
      *pkcs11_id = *pkcs11_id_str;
      *cert_config_type = ConfigType::kOpenVpn;
      return;
    }

    // Look for L2TP-IPsec specific properties.
    pkcs11_id_str = provider_properties->FindStringKey(
        shill::kL2TPIPsecClientCertIdProperty);
    if (pkcs11_id_str) {
      *pkcs11_id = *pkcs11_id_str;

      const std::string* cert_slot = provider_properties->FindStringKey(
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
        provider_properties->FindStringKey(shill::kIKEv2ClientCertIdProperty);
    if (pkcs11_id_str) {
      *pkcs11_id = *pkcs11_id_str;

      const std::string* cert_slot = provider_properties->FindStringKey(
          shill::kIKEv2ClientCertSlotProperty);
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
      shill_properties.FindStringKey(shill::kEapCertIdProperty);
  if (cert_id) {
    // Shill requires both CertID and KeyID for TLS connections, despite the
    // fact that by convention they are the same ID, because one identifies
    // the certificate and the other the private key.
    std::string key_id;
    const std::string* key_id_str =
        shill_properties.FindStringKey(shill::kEapKeyIdProperty);
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
                        base::Value* properties) {
  switch (cert_config_type) {
    case ConfigType::kNone: {
      return;
    }
    case ConfigType::kOpenVpn: {
      properties->SetKey(shill::kOpenVPNPinProperty,
                         base::Value(kDefaultTPMPin));
      // Note: OpenVPN does not have a slot property, see crbug.com/769550.
      properties->SetKey(shill::kOpenVPNClientCertIdProperty,
                         base::Value(pkcs11_id));
      break;
    }
    case ConfigType::kL2tpIpsec: {
      properties->SetKey(shill::kL2TPIPsecPinProperty,
                         base::Value(kDefaultTPMPin));
      properties->SetKey(shill::kL2TPIPsecClientCertSlotProperty,
                         base::Value(base::NumberToString(tpm_slot)));
      properties->SetKey(shill::kL2TPIPsecClientCertIdProperty,
                         base::Value(pkcs11_id));
      break;
    }
    case ConfigType::kIkev2: {
      // PIN property is not used by shill for a IKEv2 service since it is a
      // fixed value.
      properties->SetKey(shill::kIKEv2ClientCertSlotProperty,
                         base::Value(base::NumberToString(tpm_slot)));
      properties->SetKey(shill::kIKEv2ClientCertIdProperty,
                         base::Value(pkcs11_id));
      break;
    }
    case ConfigType::kEap: {
      properties->SetKey(shill::kEapPinProperty, base::Value(kDefaultTPMPin));
      std::string key_id =
          base::StringPrintf("%i:%s", tpm_slot, pkcs11_id.c_str());

      // Shill requires both CertID and KeyID for TLS connections, despite the
      // fact that by convention they are the same ID, because one identifies
      // the certificate and the other the private key.
      properties->SetKey(shill::kEapCertIdProperty, base::Value(key_id));
      properties->SetKey(shill::kEapKeyIdProperty, base::Value(key_id));
      break;
    }
  }
}

void SetEmptyShillProperties(const ConfigType cert_config_type,
                             base::Value* properties) {
  switch (cert_config_type) {
    case ConfigType::kNone: {
      return;
    }
    case ConfigType::kOpenVpn: {
      properties->SetKey(shill::kOpenVPNPinProperty,
                         base::Value(std::string()));
      properties->SetKey(shill::kOpenVPNClientCertIdProperty,
                         base::Value(std::string()));
      break;
    }
    case ConfigType::kL2tpIpsec: {
      properties->SetKey(shill::kL2TPIPsecPinProperty,
                         base::Value(std::string()));
      properties->SetKey(shill::kL2TPIPsecClientCertSlotProperty,
                         base::Value(std::string()));
      properties->SetKey(shill::kL2TPIPsecClientCertIdProperty,
                         base::Value(std::string()));
      break;
    }
    case ConfigType::kIkev2: {
      // PIN property is not used by shill for a IKEv2 service since it is a
      // fixed value.
      properties->SetKey(shill::kIKEv2ClientCertSlotProperty,
                         base::Value(std::string()));
      properties->SetKey(shill::kIKEv2ClientCertIdProperty,
                         base::Value(std::string()));
      break;
    }
    case ConfigType::kEap: {
      properties->SetKey(shill::kEapPinProperty, base::Value(std::string()));
      // Shill requires both CertID and KeyID for TLS connections, despite the
      // fact that by convention they are the same ID, because one identifies
      // the certificate and the other the private key.
      properties->SetKey(shill::kEapCertIdProperty, base::Value(std::string()));
      properties->SetKey(shill::kEapKeyIdProperty, base::Value(std::string()));
      break;
    }
  }
}

ClientCertConfig::ClientCertConfig()
    : location(ConfigType::kNone),
      client_cert_type(onc::client_cert::kClientCertTypeNone) {}

ClientCertConfig::ClientCertConfig(const ClientCertConfig& other) = default;

ClientCertConfig::~ClientCertConfig() = default;

void OncToClientCertConfig(::onc::ONCSource onc_source,
                           const base::Value& network_config,
                           ClientCertConfig* cert_config) {
  *cert_config = ClientCertConfig();

  const base::Value* dict_with_client_cert = nullptr;

  const base::Value* wifi =
      network_config.FindDictKey(::onc::network_config::kWiFi);
  if (wifi) {
    const base::Value* eap = wifi->FindDictKey(::onc::wifi::kEAP);
    if (!eap)
      return;

    dict_with_client_cert = eap;
    cert_config->location = ConfigType::kEap;
  }

  const base::Value* vpn =
      network_config.FindDictKey(::onc::network_config::kVPN);
  if (vpn) {
    const base::Value* openvpn = vpn->FindDictKey(::onc::vpn::kOpenVPN);
    const base::Value* ipsec = vpn->FindDictKey(::onc::vpn::kIPsec);
    const base::Value* l2tp = vpn->FindDictKey(::onc::vpn::kL2TP);
    if (openvpn) {
      dict_with_client_cert = openvpn;
      cert_config->location = ConfigType::kOpenVpn;
    } else if (ipsec) {
      dict_with_client_cert = ipsec;
      // Currently we support two kinds of IPsec-based VPN:
      // - L2TP/IPsec: IKE version is 1 and |l2tp| is set;
      // - IKEv2: IKE version is 2 and |l2tp| is not set.
      // Thus we only use |l2tp| to distinguish between these two cases.
      cert_config->location =
          l2tp ? ConfigType::kL2tpIpsec : ConfigType::kIkev2;
    } else {
      return;
    }
  }

  const base::Value* ethernet =
      network_config.FindDictKey(::onc::network_config::kEthernet);
  if (ethernet) {
    const base::Value* eap = ethernet->FindDictKey(::onc::wifi::kEAP);
    if (!eap)
      return;
    dict_with_client_cert = eap;
    cert_config->location = ConfigType::kEap;
  }

  if (dict_with_client_cert) {
    GetClientCertTypeAndDescriptor(onc_source, *dict_with_client_cert,
                                   cert_config);
  }
}

}  // namespace client_cert

}  // namespace chromeos
