// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_normalizer.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"

namespace chromeos {
namespace onc {

Normalizer::Normalizer(bool remove_recommended_fields)
    : remove_recommended_fields_(remove_recommended_fields) {
}

Normalizer::~Normalizer() = default;

std::unique_ptr<base::DictionaryValue> Normalizer::NormalizeObject(
    const OncValueSignature* object_signature,
    const base::DictionaryValue& onc_object) {
  CHECK(object_signature != NULL);
  bool error = false;
  std::unique_ptr<base::DictionaryValue> result =
      MapObject(*object_signature, onc_object, &error);
  DCHECK(!error);
  return result;
}

std::unique_ptr<base::DictionaryValue> Normalizer::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    bool* error) {
  std::unique_ptr<base::DictionaryValue> normalized =
      Mapper::MapObject(signature, onc_object, error);

  if (normalized.get() == NULL)
    return std::unique_ptr<base::DictionaryValue>();

  if (remove_recommended_fields_)
    normalized->RemoveWithoutPathExpansion(::onc::kRecommended, NULL);

  if (&signature == &kCertificateSignature)
    NormalizeCertificate(normalized.get());
  else if (&signature == &kEAPSignature)
    NormalizeEAP(normalized.get());
  else if (&signature == &kEthernetSignature)
    NormalizeEthernet(normalized.get());
  else if (&signature == &kIPsecSignature)
    NormalizeIPsec(normalized.get());
  else if (&signature == &kNetworkConfigurationSignature)
    NormalizeNetworkConfiguration(normalized.get());
  else if (&signature == &kOpenVPNSignature)
    NormalizeOpenVPN(normalized.get());
  else if (&signature == &kProxySettingsSignature)
    NormalizeProxySettings(normalized.get());
  else if (&signature == &kVPNSignature)
    NormalizeVPN(normalized.get());
  else if (&signature == &kWiFiSignature)
    NormalizeWiFi(normalized.get());

  return normalized;
}

namespace {

void RemoveEntryUnless(base::Value* dict,
                       const std::string& path,
                       bool condition) {
  DCHECK(dict->is_dict());
  if (!condition && dict->FindKey(path)) {
    NET_LOG(ERROR) << "onc::Normalizer:Removing: " << path;
    dict->RemoveKey(path);
  }
}

bool IsIpConfigTypeStatic(base::Value* network,
                          const std::string& ip_config_type_key) {
  DCHECK(network->is_dict());
  base::Value* ip_config_type =
      network->FindKeyOfType(ip_config_type_key, base::Value::Type::STRING);

  return ip_config_type && ip_config_type->GetString() ==
                               ::onc::network_config::kIPConfigTypeStatic;
}

}  // namespace

void Normalizer::NormalizeCertificate(base::DictionaryValue* cert) {
  std::string type;
  cert->GetStringWithoutPathExpansion(::onc::certificate::kType, &type);
  RemoveEntryUnless(cert, ::onc::certificate::kPKCS12,
                    type == ::onc::certificate::kClient);
  RemoveEntryUnless(cert, ::onc::certificate::kTrustBits,
                    type == ::onc::certificate::kServer ||
                        type == ::onc::certificate::kAuthority);
  RemoveEntryUnless(cert, ::onc::certificate::kX509,
                    type == ::onc::certificate::kServer ||
                        type == ::onc::certificate::kAuthority);
}

void Normalizer::NormalizeEthernet(base::DictionaryValue* ethernet) {
  std::string auth;
  ethernet->GetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                          &auth);
  RemoveEntryUnless(ethernet, ::onc::ethernet::kEAP,
                    auth == ::onc::ethernet::k8021X);
}

void Normalizer::NormalizeEAP(base::DictionaryValue* eap) {
  std::string clientcert_type;
  eap->GetStringWithoutPathExpansion(::onc::client_cert::kClientCertType,
                                     &clientcert_type);
  RemoveEntryUnless(eap,
                    ::onc::client_cert::kClientCertPattern,
                    clientcert_type == ::onc::client_cert::kPattern);
  RemoveEntryUnless(eap,
                    ::onc::client_cert::kClientCertRef,
                    clientcert_type == ::onc::client_cert::kRef);

  std::string outer;
  eap->GetStringWithoutPathExpansion(::onc::eap::kOuter, &outer);
  RemoveEntryUnless(
      eap, ::onc::eap::kAnonymousIdentity,
      outer == ::onc::eap::kPEAP || outer == ::onc::eap::kEAP_TTLS);
  RemoveEntryUnless(eap, ::onc::eap::kInner,
                    outer == ::onc::eap::kPEAP ||
                        outer == ::onc::eap::kEAP_TTLS ||
                        outer == ::onc::eap::kEAP_FAST);
}

void Normalizer::NormalizeIPsec(base::DictionaryValue* ipsec) {
  std::string auth_type;
  ipsec->GetStringWithoutPathExpansion(::onc::ipsec::kAuthenticationType,
                                       &auth_type);
  RemoveEntryUnless(ipsec, ::onc::client_cert::kClientCertType,
                    auth_type == ::onc::ipsec::kCert);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kServerCARef,
                    auth_type == ::onc::ipsec::kCert);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kPSK, auth_type == ::onc::ipsec::kPSK);
  RemoveEntryUnless(ipsec, ::onc::vpn::kSaveCredentials,
                    auth_type == ::onc::ipsec::kPSK);

  std::string clientcert_type;
  ipsec->GetStringWithoutPathExpansion(::onc::client_cert::kClientCertType,
                                       &clientcert_type);
  RemoveEntryUnless(ipsec,
                    ::onc::client_cert::kClientCertPattern,
                    clientcert_type == ::onc::client_cert::kPattern);
  RemoveEntryUnless(ipsec,
                    ::onc::client_cert::kClientCertRef,
                    clientcert_type == ::onc::client_cert::kRef);

  int ike_version = -1;
  ipsec->GetIntegerWithoutPathExpansion(::onc::ipsec::kIKEVersion,
                                        &ike_version);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kEAP, ike_version == 2);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kGroup, ike_version == 1);
  RemoveEntryUnless(ipsec, ::onc::ipsec::kXAUTH, ike_version == 1);
}

void Normalizer::NormalizeNetworkConfiguration(base::DictionaryValue* network) {
  bool remove = false;
  network->GetBooleanWithoutPathExpansion(::onc::kRemove, &remove);
  if (remove) {
    network->RemoveWithoutPathExpansion(::onc::network_config::kStaticIPConfig,
                                        NULL);
    network->RemoveWithoutPathExpansion(::onc::network_config::kName, NULL);
    network->RemoveWithoutPathExpansion(::onc::network_config::kProxySettings,
                                        NULL);
    network->RemoveWithoutPathExpansion(::onc::network_config::kType, NULL);
    // Fields dependent on kType are removed afterwards, too.
  }

  std::string type;
  network->GetStringWithoutPathExpansion(::onc::network_config::kType, &type);
  RemoveEntryUnless(network,
                    ::onc::network_config::kEthernet,
                    type == ::onc::network_type::kEthernet);
  RemoveEntryUnless(
      network, ::onc::network_config::kVPN, type == ::onc::network_type::kVPN);
  RemoveEntryUnless(network,
                    ::onc::network_config::kWiFi,
                    type == ::onc::network_type::kWiFi);

  NormalizeStaticIPConfigForNetwork(network);
}

void Normalizer::NormalizeOpenVPN(base::DictionaryValue* openvpn) {
  std::string clientcert_type;
  openvpn->GetStringWithoutPathExpansion(::onc::client_cert::kClientCertType,
                                         &clientcert_type);
  RemoveEntryUnless(openvpn,
                    ::onc::client_cert::kClientCertPattern,
                    clientcert_type == ::onc::client_cert::kPattern);
  RemoveEntryUnless(openvpn,
                    ::onc::client_cert::kClientCertRef,
                    clientcert_type == ::onc::client_cert::kRef);

  base::Value* user_auth_type_value = openvpn->FindKeyOfType(
      ::onc::openvpn::kUserAuthenticationType, base::Value::Type::STRING);
  // If UserAuthenticationType is unspecified, do not strip Password and OTP.
  if (!user_auth_type_value)
    return;
  std::string user_auth_type = user_auth_type_value->GetString();
  RemoveEntryUnless(
      openvpn,
      ::onc::openvpn::kPassword,
      user_auth_type == ::onc::openvpn_user_auth_type::kPassword ||
          user_auth_type == ::onc::openvpn_user_auth_type::kPasswordAndOTP);
  RemoveEntryUnless(
      openvpn,
      ::onc::openvpn::kOTP,
      user_auth_type == ::onc::openvpn_user_auth_type::kOTP ||
          user_auth_type == ::onc::openvpn_user_auth_type::kPasswordAndOTP);
}

void Normalizer::NormalizeProxySettings(base::DictionaryValue* proxy) {
  std::string type;
  proxy->GetStringWithoutPathExpansion(::onc::proxy::kType, &type);
  RemoveEntryUnless(proxy, ::onc::proxy::kManual,
                    type == ::onc::proxy::kManual);
  RemoveEntryUnless(proxy, ::onc::proxy::kExcludeDomains,
                    type == ::onc::proxy::kManual);
  RemoveEntryUnless(proxy, ::onc::proxy::kPAC, type == ::onc::proxy::kPAC);
}

void Normalizer::NormalizeVPN(base::DictionaryValue* vpn) {
  std::string type;
  vpn->GetStringWithoutPathExpansion(::onc::vpn::kType, &type);
  RemoveEntryUnless(vpn, ::onc::vpn::kOpenVPN, type == ::onc::vpn::kOpenVPN);
  RemoveEntryUnless(
      vpn, ::onc::vpn::kIPsec,
      type == ::onc::vpn::kIPsec || type == ::onc::vpn::kTypeL2TP_IPsec);
  RemoveEntryUnless(vpn, ::onc::vpn::kL2TP,
                    type == ::onc::vpn::kTypeL2TP_IPsec);
  RemoveEntryUnless(vpn, ::onc::vpn::kThirdPartyVpn,
                    type == ::onc::vpn::kThirdPartyVpn);
  RemoveEntryUnless(vpn, ::onc::vpn::kArcVpn, type == ::onc::vpn::kArcVpn);
}

void Normalizer::NormalizeWiFi(base::DictionaryValue* wifi) {
  std::string security;
  wifi->GetStringWithoutPathExpansion(::onc::wifi::kSecurity, &security);
  RemoveEntryUnless(
      wifi, ::onc::wifi::kEAP,
      security == ::onc::wifi::kWEP_8021X || security == ::onc::wifi::kWPA_EAP);
  RemoveEntryUnless(
      wifi, ::onc::wifi::kPassphrase,
      security == ::onc::wifi::kWEP_PSK || security == ::onc::wifi::kWPA_PSK);
  FillInHexSSIDField(wifi);
}

void Normalizer::NormalizeStaticIPConfigForNetwork(
    base::DictionaryValue* network) {
  const bool ip_config_type_is_static = IsIpConfigTypeStatic(
      network, ::onc::network_config::kIPAddressConfigType);
  const bool name_servers_type_is_static = IsIpConfigTypeStatic(
      network, ::onc::network_config::kNameServersConfigType);

  RemoveEntryUnless(network, ::onc::network_config::kStaticIPConfig,
                    ip_config_type_is_static || name_servers_type_is_static);

  base::Value* static_ip_config = network->FindKeyOfType(
      ::onc::network_config::kStaticIPConfig, base::Value::Type::DICTIONARY);
  bool all_ip_fields_exist = false;
  bool name_servers_exist = false;
  if (static_ip_config) {
    all_ip_fields_exist =
        static_ip_config->FindKey(::onc::ipconfig::kIPAddress) &&
        static_ip_config->FindKey(::onc::ipconfig::kGateway) &&
        static_ip_config->FindKey(::onc::ipconfig::kRoutingPrefix);

    name_servers_exist =
        static_ip_config->FindKey(::onc::ipconfig::kNameServers);

    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kIPAddress,
                      all_ip_fields_exist && ip_config_type_is_static);
    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kGateway,
                      all_ip_fields_exist && ip_config_type_is_static);
    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kRoutingPrefix,
                      all_ip_fields_exist && ip_config_type_is_static);

    RemoveEntryUnless(static_ip_config, ::onc::ipconfig::kNameServers,
                      name_servers_type_is_static);

    RemoveEntryUnless(network, ::onc::network_config::kStaticIPConfig,
                      !static_ip_config->DictEmpty());
  }

  RemoveEntryUnless(network, ::onc::network_config::kIPAddressConfigType,
                    !ip_config_type_is_static || all_ip_fields_exist);
  RemoveEntryUnless(network, ::onc::network_config::kNameServersConfigType,
                    !name_servers_type_is_static || name_servers_exist);
}

}  // namespace onc
}  // namespace chromeos
