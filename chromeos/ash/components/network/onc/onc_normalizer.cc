// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_normalizer.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_constants.h"

namespace ash::onc {

Normalizer::Normalizer(bool remove_recommended_fields)
    : remove_recommended_fields_(remove_recommended_fields) {}

Normalizer::~Normalizer() = default;

base::Value::Dict Normalizer::NormalizeObject(
    const chromeos::onc::OncValueSignature* object_signature,
    const base::Value::Dict& onc_object) {
  CHECK(object_signature != nullptr);
  bool error = false;
  base::Value::Dict result = MapObject(*object_signature, onc_object, &error);
  DCHECK(!error);
  return result;
}

base::Value::Dict Normalizer::MapObject(
    const chromeos::onc::OncValueSignature& signature,
    const base::Value::Dict& onc_object,
    bool* error) {
  base::Value::Dict normalized =
      chromeos::onc::Mapper::MapObject(signature, onc_object, error);

  if (*error) {
    return {};
  }

  if (remove_recommended_fields_)
    normalized.Remove(::onc::kRecommended);

  if (&signature == &chromeos::onc::kCertificateSignature)
    NormalizeCertificate(&normalized);
  else if (&signature == &chromeos::onc::kEAPSignature)
    NormalizeEAP(&normalized);
  else if (&signature == &chromeos::onc::kEthernetSignature)
    NormalizeEthernet(&normalized);
  else if (&signature == &chromeos::onc::kIPsecSignature)
    NormalizeIPsec(&normalized);
  else if (&signature == &chromeos::onc::kNetworkConfigurationSignature)
    NormalizeNetworkConfiguration(&normalized);
  else if (&signature == &chromeos::onc::kOpenVPNSignature)
    NormalizeOpenVPN(&normalized);
  else if (&signature == &chromeos::onc::kProxySettingsSignature)
    NormalizeProxySettings(&normalized);
  else if (&signature == &chromeos::onc::kVPNSignature)
    NormalizeVPN(&normalized);
  else if (&signature == &chromeos::onc::kWiFiSignature)
    NormalizeWiFi(&normalized);

  return normalized;
}

namespace {

void RemoveEntryUnless(base::Value::Dict* dict,
                       const std::string& path,
                       bool condition) {
  if (!condition && dict->contains(path)) {
    NET_LOG(ERROR) << "onc::Normalizer:Removing: " << path;
    dict->Remove(path);
  }
}

bool IsIpConfigTypeStatic(base::Value::Dict* network,
                          const std::string& ip_config_type_key) {
  std::string* ip_config_type = network->FindString(ip_config_type_key);
  return ip_config_type &&
         (*ip_config_type) == ::onc::network_config::kIPConfigTypeStatic;
}

std::string GetString(const base::Value::Dict& dict, const char* key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

}  // namespace

void Normalizer::NormalizeCertificate(base::Value::Dict* cert) {
  std::string type = GetString(*cert, ::onc::certificate::kType);
  RemoveEntryUnless(cert, ::onc::certificate::kPKCS12,
                    type == ::onc::certificate::kClient);
  RemoveEntryUnless(cert, ::onc::certificate::kTrustBits,
                    type == ::onc::certificate::kServer ||
                        type == ::onc::certificate::kAuthority);
  RemoveEntryUnless(cert, ::onc::certificate::kX509,
                    type == ::onc::certificate::kServer ||
                        type == ::onc::certificate::kAuthority);
}

void Normalizer::NormalizeEthernet(base::Value::Dict* ethernet) {
  std::string auth = GetString(*ethernet, ::onc::ethernet::kAuthentication);
  RemoveEntryUnless(ethernet, ::onc::ethernet::kEAP,
                    auth == ::onc::ethernet::k8021X);
}

void Normalizer::NormalizeEAP(base::Value::Dict* eap) {
  std::string clientcert_type =
      GetString(*eap, ::onc::client_cert::kClientCertType);
  RemoveEntryUnless(eap, ::onc::client_cert::kClientCertPattern,
                    clientcert_type == ::onc::client_cert::kPattern);
  RemoveEntryUnless(eap, ::onc::client_cert::kClientCertRef,
                    clientcert_type == ::onc::client_cert::kRef);
  RemoveEntryUnless(
      eap, ::onc::client_cert::kClientCertProvisioningProfileId,
      clientcert_type == ::onc::client_cert::kProvisioningProfileId);

  std::string outer = GetString(*eap, ::onc::eap::kOuter);
  RemoveEntryUnless(
      eap, ::onc::eap::kAnonymousIdentity,
      outer == ::onc::eap::kPEAP || outer == ::onc::eap::kEAP_TTLS);
  RemoveEntryUnless(eap, ::onc::eap::kInner,
                    outer == ::onc::eap::kPEAP ||
                        outer == ::onc::eap::kEAP_TTLS ||
                        outer == ::onc::eap::kEAP_FAST);
}

void Normalizer::NormalizeIPsec(base::Value::Dict* ipsec) {
  std::string auth_type = GetString(*ipsec, ::onc::ipsec::kAuthenticationType);
  RemoveEntryUnless(ipsec, ::onc::client_cert::kClientCertType,
                    auth_type == ::onc::ipsec::kCert);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kServerCARef,
                    auth_type == ::onc::ipsec::kCert);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kPSK, auth_type == ::onc::ipsec::kPSK);
  RemoveEntryUnless(ipsec, ::onc::vpn::kSaveCredentials,
                    auth_type == ::onc::ipsec::kPSK);

  std::string clientcert_type =
      GetString(*ipsec, ::onc::client_cert::kClientCertType);
  RemoveEntryUnless(ipsec, ::onc::client_cert::kClientCertPattern,
                    clientcert_type == ::onc::client_cert::kPattern);
  RemoveEntryUnless(ipsec, ::onc::client_cert::kClientCertRef,
                    clientcert_type == ::onc::client_cert::kRef);
  RemoveEntryUnless(
      ipsec, ::onc::client_cert::kClientCertProvisioningProfileId,
      clientcert_type == ::onc::client_cert::kProvisioningProfileId);

  int ike_version = ipsec->FindInt(::onc::ipsec::kIKEVersion).value_or(-1);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kEAP, ike_version == 2);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kGroup, ike_version == 1);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kXAUTH, ike_version == 1);
}

void Normalizer::NormalizeNetworkConfiguration(base::Value::Dict* network) {
  bool remove = network->FindBool(::onc::kRemove).value_or(false);
  if (remove) {
    network->Remove(::onc::network_config::kStaticIPConfig);
    network->Remove(::onc::network_config::kName);
    network->Remove(::onc::network_config::kProxySettings);
    network->Remove(::onc::network_config::kType);
    // Fields dependent on kType are removed afterwards, too.
  }

  std::string type = GetString(*network, ::onc::network_config::kType);
  RemoveEntryUnless(network, ::onc::network_config::kEthernet,
                    type == ::onc::network_type::kEthernet);
  RemoveEntryUnless(network, ::onc::network_config::kVPN,
                    type == ::onc::network_type::kVPN);
  RemoveEntryUnless(network, ::onc::network_config::kWiFi,
                    type == ::onc::network_type::kWiFi);

  NormalizeStaticIPConfigForNetwork(network);
}

void Normalizer::NormalizeOpenVPN(base::Value::Dict* openvpn) {
  std::string clientcert_type =
      GetString(*openvpn, ::onc::client_cert::kClientCertType);
  RemoveEntryUnless(openvpn, ::onc::client_cert::kClientCertPattern,
                    clientcert_type == ::onc::client_cert::kPattern);
  RemoveEntryUnless(openvpn, ::onc::client_cert::kClientCertRef,
                    clientcert_type == ::onc::client_cert::kRef);
  RemoveEntryUnless(
      openvpn, ::onc::client_cert::kClientCertProvisioningProfileId,
      clientcert_type == ::onc::client_cert::kProvisioningProfileId);

  const std::string* user_auth_type =
      openvpn->FindString(::onc::openvpn::kUserAuthenticationType);
  // If UserAuthenticationType is unspecified, do not strip Password and OTP.
  if (user_auth_type) {
    RemoveEntryUnless(
        openvpn, ::onc::openvpn::kPassword,
        *user_auth_type == ::onc::openvpn_user_auth_type::kPassword ||
            *user_auth_type == ::onc::openvpn_user_auth_type::kPasswordAndOTP);
    RemoveEntryUnless(
        openvpn, ::onc::openvpn::kOTP,
        *user_auth_type == ::onc::openvpn_user_auth_type::kOTP ||
            *user_auth_type == ::onc::openvpn_user_auth_type::kPasswordAndOTP);
  }

  const std::string* compression_algorithm =
      openvpn->FindString(::onc::openvpn::kCompressionAlgorithm);
  if (compression_algorithm) {
    RemoveEntryUnless(
        openvpn, ::onc::openvpn::kCompressionAlgorithm,
        *compression_algorithm != ::onc::openvpn_compression_algorithm::kNone);
  }
}

void Normalizer::NormalizeProxySettings(base::Value::Dict* proxy) {
  std::string type = GetString(*proxy, ::onc::proxy::kType);
  RemoveEntryUnless(proxy, ::onc::proxy::kManual,
                    type == ::onc::proxy::kManual);
  RemoveEntryUnless(proxy, ::onc::proxy::kExcludeDomains,
                    type == ::onc::proxy::kManual);
  RemoveEntryUnless(proxy, ::onc::proxy::kPAC, type == ::onc::proxy::kPAC);
}

void Normalizer::NormalizeVPN(base::Value::Dict* vpn) {
  std::string type = GetString(*vpn, ::onc::vpn::kType);
  RemoveEntryUnless(vpn, ::onc::vpn::kOpenVPN, type == ::onc::vpn::kOpenVPN);
  RemoveEntryUnless(vpn, ::onc::vpn::kWireGuard,
                    type == ::onc::vpn::kWireGuard);
  RemoveEntryUnless(
      vpn, ::onc::vpn::kIPsec,
      type == ::onc::vpn::kIPsec || type == ::onc::vpn::kTypeL2TP_IPsec);
  RemoveEntryUnless(vpn, ::onc::vpn::kL2TP,
                    type == ::onc::vpn::kTypeL2TP_IPsec);
  RemoveEntryUnless(vpn, ::onc::vpn::kThirdPartyVpn,
                    type == ::onc::vpn::kThirdPartyVpn);
  RemoveEntryUnless(vpn, ::onc::vpn::kArcVpn, type == ::onc::vpn::kArcVpn);
}

void Normalizer::NormalizeWiFi(base::Value::Dict* wifi) {
  std::string security = GetString(*wifi, ::onc::wifi::kSecurity);
  RemoveEntryUnless(
      wifi, ::onc::wifi::kEAP,
      security == ::onc::wifi::kWEP_8021X || security == ::onc::wifi::kWPA_EAP);
  RemoveEntryUnless(
      wifi, ::onc::wifi::kPassphrase,
      security == ::onc::wifi::kWEP_PSK || security == ::onc::wifi::kWPA_PSK);
  chromeos::onc::FillInHexSSIDField(*wifi);
}

void Normalizer::NormalizeStaticIPConfigForNetwork(base::Value::Dict* network) {
  const bool ip_config_type_is_static = IsIpConfigTypeStatic(
      network, ::onc::network_config::kIPAddressConfigType);
  const bool name_servers_type_is_static = IsIpConfigTypeStatic(
      network, ::onc::network_config::kNameServersConfigType);

  base::Value::Dict* static_ip_config =
      network->FindDict(::onc::network_config::kStaticIPConfig);
  bool all_ip_fields_exist = false;
  bool name_servers_exist = false;
  if (static_ip_config) {
    all_ip_fields_exist =
        static_ip_config->contains(::onc::ipconfig::kIPAddress) &&
        static_ip_config->contains(::onc::ipconfig::kGateway) &&
        static_ip_config->contains(::onc::ipconfig::kRoutingPrefix);

    name_servers_exist =
        static_ip_config->contains(::onc::ipconfig::kNameServers);

    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kIPAddress,
                      all_ip_fields_exist && ip_config_type_is_static);
    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kGateway,
                      all_ip_fields_exist && ip_config_type_is_static);
    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kRoutingPrefix,
                      all_ip_fields_exist && ip_config_type_is_static);

    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kNameServers,
                      name_servers_type_is_static);

    RemoveEntryUnless(network, ::onc::network_config::kStaticIPConfig,
                      !static_ip_config->empty());
  }

  RemoveEntryUnless(network, ::onc::network_config::kIPAddressConfigType,
                    !ip_config_type_is_static || all_ip_fields_exist);
  RemoveEntryUnless(network, ::onc::network_config::kNameServersConfigType,
                    !name_servers_type_is_static || name_servers_exist);
}

}  // namespace ash::onc
