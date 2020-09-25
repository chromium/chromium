// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_mapper.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "chromeos/network/tether_constants.h"
#include "components/account_id/account_id.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/cert/pem.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromeos {
namespace onc {

namespace {

// Error messages that can be reported when decrypting encrypted ONC.
constexpr char kUnableToDecrypt[] = "Unable to decrypt encrypted ONC";
constexpr char kUnableToDecode[] = "Unable to decode encrypted ONC";

// Scheme strings for supported |net::ProxyServer::SCHEME_*| enum values.
constexpr char kDirectScheme[] = "direct";
constexpr char kQuicScheme[] = "quic";
constexpr char kSocksScheme[] = "socks";
constexpr char kSocks4Scheme[] = "socks4";
constexpr char kSocks5Scheme[] = "socks5";

std::string GetString(const base::Value& dict, const char* key) {
  const base::Value* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  if (!value)
    return std::string();
  return value->GetString();
}

bool GetString(const base::Value& dict, const char* key, std::string* result) {
  const base::Value* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  if (!value)
    return false;
  *result = value->GetString();
  return true;
}

int GetInt(const base::Value& dict, const char* key, int default_value) {
  const base::Value* value =
      dict.FindKeyOfType(key, base::Value::Type::INTEGER);
  if (!value)
    return default_value;
  return value->GetInt();
}

bool GetInt(const base::Value& dict, const char* key, int* result) {
  const base::Value* value =
      dict.FindKeyOfType(key, base::Value::Type::INTEGER);
  if (!value)
    return false;
  *result = value->GetInt();
  return true;
}

// Runs |variable_expander.ExpandString| on the field |fieldname| in
// |onc_object|.
void ExpandField(const std::string& fieldname,
                 const VariableExpander& variable_expander,
                 base::DictionaryValue* onc_object) {
  std::string field_value;
  if (!onc_object->GetStringWithoutPathExpansion(fieldname, &field_value))
    return;

  variable_expander.ExpandString(&field_value);

  onc_object->SetKey(fieldname, base::Value(field_value));
}

// A |Mapper| for masking sensitive fields (e.g. credentials such as
// passphrases) in ONC.
class OncMaskValues : public Mapper {
 public:
  static std::unique_ptr<base::DictionaryValue> Mask(
      const OncValueSignature& signature,
      const base::DictionaryValue& onc_object,
      const std::string& mask) {
    OncMaskValues masker(mask);
    bool unused_error;
    return masker.MapObject(signature, onc_object, &unused_error);
  }

 protected:
  explicit OncMaskValues(const std::string& mask) : mask_(mask) {}

  std::unique_ptr<base::Value> MapField(
      const std::string& field_name,
      const OncValueSignature& object_signature,
      const base::Value& onc_value,
      bool* found_unknown_field,
      bool* error) override {
    if (FieldIsCredential(object_signature, field_name)) {
      // If it's the password field and the substitution string is used, don't
      // mask it.
      if (&object_signature == &kEAPSignature &&
          field_name == ::onc::eap::kPassword &&
          onc_value.GetString() ==
              ::onc::substitutes::kPasswordPlaceholderVerbatim) {
        return Mapper::MapField(field_name, object_signature, onc_value,
                                found_unknown_field, error);
      }
      return std::unique_ptr<base::Value>(new base::Value(mask_));
    } else {
      return Mapper::MapField(field_name, object_signature, onc_value,
                              found_unknown_field, error);
    }
  }

  // Mask to insert in place of the sensitive values.
  std::string mask_;
};

// Returns a map GUID->PEM of all server and authority certificates defined in
// the Certificates section of ONC, which is passed in as |certificates|.
CertPEMsByGUIDMap GetServerAndCACertsByGUID(
    const base::ListValue& certificates) {
  CertPEMsByGUIDMap certs_by_guid;
  for (const auto& entry : certificates) {
    const base::DictionaryValue* cert = nullptr;
    bool entry_is_dictionary = entry.GetAsDictionary(&cert);
    DCHECK(entry_is_dictionary);

    std::string guid;
    cert->GetStringWithoutPathExpansion(::onc::certificate::kGUID, &guid);
    std::string cert_type;
    cert->GetStringWithoutPathExpansion(::onc::certificate::kType, &cert_type);
    if (cert_type != ::onc::certificate::kServer &&
        cert_type != ::onc::certificate::kAuthority) {
      continue;
    }
    std::string x509_data;
    cert->GetStringWithoutPathExpansion(::onc::certificate::kX509, &x509_data);

    std::string der = DecodePEM(x509_data);
    std::string pem;
    if (der.empty() || !net::X509Certificate::GetPEMEncodedFromDER(der, &pem)) {
      LOG(ERROR) << "Certificate with GUID " << guid
                 << " is not in PEM encoding.";
      continue;
    }
    certs_by_guid[guid] = pem;
  }

  return certs_by_guid;
}

// Fills HexSSID fields in all entries in the |network_configs| list.
void FillInHexSSIDFieldsInNetworks(base::Value* network_configs) {
  for (auto& network : network_configs->GetList())
    FillInHexSSIDFieldsInOncObject(kNetworkConfigurationSignature, &network);
}

// Given a GUID->PEM certificate mapping |certs_by_guid|, looks up the PEM
// encoded certificate referenced by |guid_ref|. If a match is found, sets
// |*pem_encoded| to the PEM encoded certificate and returns true. Otherwise,
// returns false.
bool GUIDRefToPEMEncoding(const CertPEMsByGUIDMap& certs_by_guid,
                          const std::string& guid_ref,
                          std::string* pem_encoded) {
  CertPEMsByGUIDMap::const_iterator it = certs_by_guid.find(guid_ref);
  if (it == certs_by_guid.end()) {
    LOG(ERROR) << "Couldn't resolve certificate reference " << guid_ref;
    return false;
  }
  *pem_encoded = it->second;
  if (pem_encoded->empty()) {
    LOG(ERROR) << "Couldn't PEM-encode certificate with GUID " << guid_ref;
    return false;
  }
  return true;
}

// Given a GUID-> PM certificate mapping |certs_by_guid|, attempts to resolve
// the certificate referenced by the |key_guid_ref| field in |onc_object|.
// * If |onc_object| has no |key_guid_ref| field, returns true.
// * If no matching certificate is found in |certs_by_guid|, returns false.
// * If a matching certificate is found, removes the |key_guid_ref| field,
//   fills the |key_pem| field in |onc_object| and returns true.
bool ResolveSingleCertRef(const CertPEMsByGUIDMap& certs_by_guid,
                          const std::string& key_guid_ref,
                          const std::string& key_pem,
                          base::DictionaryValue* onc_object) {
  std::string guid_ref;
  if (!onc_object->GetStringWithoutPathExpansion(key_guid_ref, &guid_ref))
    return true;

  std::string pem_encoded;
  if (!GUIDRefToPEMEncoding(certs_by_guid, guid_ref, &pem_encoded))
    return false;

  onc_object->RemoveKey(key_guid_ref);
  onc_object->SetKey(key_pem, base::Value(pem_encoded));
  return true;
}

// Given a GUID-> PM certificate mapping |certs_by_guid|, attempts to resolve
// the certificates referenced by the list-of-strings field |key_guid_ref_list|
// in |onc_object|.
// * If |key_guid_ref_list| does not exist in |onc_object|, returns true.
// * If any element |key_guid_ref_list| can not be found in |certs_by_guid|,
//   aborts processing and returns false. |onc_object| is unchanged in this
//   case.
// * Otherwise, sets |key_pem_list| to be a list-of-strings field in
//   |onc_object|, containing all PEM encoded resolved certificates in order and
//   returns true.
bool ResolveCertRefList(const CertPEMsByGUIDMap& certs_by_guid,
                        const std::string& key_guid_ref_list,
                        const std::string& key_pem_list,
                        base::DictionaryValue* onc_object) {
  const base::ListValue* guid_ref_list = nullptr;
  if (!onc_object->GetListWithoutPathExpansion(key_guid_ref_list,
                                               &guid_ref_list)) {
    return true;
  }

  std::unique_ptr<base::ListValue> pem_list(new base::ListValue);
  for (const auto& entry : *guid_ref_list) {
    std::string guid_ref;
    bool entry_is_string = entry.GetAsString(&guid_ref);
    DCHECK(entry_is_string);

    std::string pem_encoded;
    if (!GUIDRefToPEMEncoding(certs_by_guid, guid_ref, &pem_encoded))
      return false;

    pem_list->AppendString(pem_encoded);
  }

  onc_object->RemoveKey(key_guid_ref_list);
  onc_object->SetWithoutPathExpansion(key_pem_list, std::move(pem_list));
  return true;
}

// Same as |ResolveSingleCertRef|, but the output |key_pem_list| will be set to
// a list with exactly one value when resolution succeeds.
bool ResolveSingleCertRefToList(const CertPEMsByGUIDMap& certs_by_guid,
                                const std::string& key_guid_ref,
                                const std::string& key_pem_list,
                                base::DictionaryValue* onc_object) {
  std::string guid_ref;
  if (!onc_object->GetStringWithoutPathExpansion(key_guid_ref, &guid_ref))
    return true;

  std::string pem_encoded;
  if (!GUIDRefToPEMEncoding(certs_by_guid, guid_ref, &pem_encoded))
    return false;

  std::unique_ptr<base::ListValue> pem_list(new base::ListValue);
  pem_list->AppendString(pem_encoded);
  onc_object->RemoveKey(key_guid_ref);
  onc_object->SetWithoutPathExpansion(key_pem_list, std::move(pem_list));
  return true;
}

// Resolves the reference list at |key_guid_refs| if present and otherwise the
// single reference at |key_guid_ref|. Returns whether the respective resolving
// was successful.
bool ResolveCertRefsOrRefToList(const CertPEMsByGUIDMap& certs_by_guid,
                                const std::string& key_guid_refs,
                                const std::string& key_guid_ref,
                                const std::string& key_pem_list,
                                base::DictionaryValue* onc_object) {
  if (onc_object->HasKey(key_guid_refs)) {
    if (onc_object->HasKey(key_guid_ref)) {
      LOG(ERROR) << "Found both " << key_guid_refs << " and " << key_guid_ref
                 << ". Ignoring and removing the latter.";
      onc_object->RemoveKey(key_guid_ref);
    }
    return ResolveCertRefList(certs_by_guid, key_guid_refs, key_pem_list,
                              onc_object);
  }

  // Only resolve |key_guid_ref| if |key_guid_refs| isn't present.
  return ResolveSingleCertRefToList(certs_by_guid, key_guid_ref, key_pem_list,
                                    onc_object);
}

// Resolve known server and authority certiifcate reference fields in
// |onc_object|.
bool ResolveServerCertRefsInObject(const CertPEMsByGUIDMap& certs_by_guid,
                                   const OncValueSignature& signature,
                                   base::DictionaryValue* onc_object) {
  if (&signature == &kCertificatePatternSignature) {
    if (!ResolveCertRefList(certs_by_guid, ::onc::client_cert::kIssuerCARef,
                            ::onc::client_cert::kIssuerCAPEMs, onc_object)) {
      return false;
    }
  } else if (&signature == &kEAPSignature) {
    if (!ResolveCertRefsOrRefToList(certs_by_guid, ::onc::eap::kServerCARefs,
                                    ::onc::eap::kServerCARef,
                                    ::onc::eap::kServerCAPEMs, onc_object)) {
      return false;
    }
  } else if (&signature == &kIPsecSignature) {
    if (!ResolveCertRefsOrRefToList(certs_by_guid, ::onc::ipsec::kServerCARefs,
                                    ::onc::ipsec::kServerCARef,
                                    ::onc::ipsec::kServerCAPEMs, onc_object)) {
      return false;
    }
  } else if (&signature == &kIPsecSignature ||
             &signature == &kOpenVPNSignature) {
    if (!ResolveSingleCertRef(certs_by_guid, ::onc::openvpn::kServerCertRef,
                              ::onc::openvpn::kServerCertPEM, onc_object) ||
        !ResolveCertRefsOrRefToList(
            certs_by_guid, ::onc::openvpn::kServerCARefs,
            ::onc::openvpn::kServerCARef, ::onc::openvpn::kServerCAPEMs,
            onc_object)) {
      return false;
    }
  }

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(*onc_object); !it.IsAtEnd();
       it.Advance()) {
    base::DictionaryValue* inner_object = nullptr;
    if (!onc_object->GetDictionaryWithoutPathExpansion(it.key(), &inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());
    if (!field_signature)
      continue;

    if (!ResolveServerCertRefsInObject(
            certs_by_guid, *field_signature->value_signature, inner_object)) {
      return false;
    }
  }
  return true;
}

net::ProxyServer ConvertOncProxyLocationToHostPort(
    net::ProxyServer::Scheme default_proxy_scheme,
    const base::Value& onc_proxy_location) {
  std::string host = GetString(onc_proxy_location, ::onc::proxy::kHost);
  // Parse |host| according to the format [<scheme>"://"]<server>[":"<port>].
  net::ProxyServer proxy_server =
      net::ProxyServer::FromURI(host, default_proxy_scheme);
  int port = GetInt(onc_proxy_location, ::onc::proxy::kPort, 0);

  // Replace the port parsed from |host| by the provided |port|.
  return net::ProxyServer(
      proxy_server.scheme(),
      net::HostPortPair(proxy_server.host_port_pair().host(),
                        static_cast<uint16_t>(port)));
}

void AppendProxyServerForScheme(const base::Value& onc_manual,
                                const std::string& onc_scheme,
                                std::string* spec) {
  const base::Value* onc_proxy_location = onc_manual.FindKey(onc_scheme);
  if (!onc_proxy_location)
    return;

  net::ProxyServer::Scheme default_proxy_scheme = net::ProxyServer::SCHEME_HTTP;
  std::string url_scheme;
  if (onc_scheme == ::onc::proxy::kFtp) {
    url_scheme = url::kFtpScheme;
  } else if (onc_scheme == ::onc::proxy::kHttp) {
    url_scheme = url::kHttpScheme;
  } else if (onc_scheme == ::onc::proxy::kHttps) {
    url_scheme = url::kHttpsScheme;
  } else if (onc_scheme == ::onc::proxy::kSocks) {
    default_proxy_scheme = net::ProxyServer::SCHEME_SOCKS4;
    url_scheme = kSocksScheme;
  } else {
    NOTREACHED();
  }

  net::ProxyServer proxy_server = ConvertOncProxyLocationToHostPort(
      default_proxy_scheme, *onc_proxy_location);

  ProxyConfigDictionary::EncodeAndAppendProxyServer(url_scheme, proxy_server,
                                                    spec);
}

net::ProxyBypassRules ConvertOncExcludeDomainsToBypassRules(
    const base::Value& onc_exclude_domains) {
  net::ProxyBypassRules rules;
  for (const base::Value& value : onc_exclude_domains.GetList()) {
    if (!value.is_string()) {
      LOG(ERROR) << "Badly formatted ONC exclude domains";
      continue;
    }
    rules.AddRuleFromString(value.GetString());
  }
  return rules;
}

std::string SchemeToString(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_DIRECT:
      return kDirectScheme;
    case net::ProxyServer::SCHEME_HTTP:
      return url::kHttpScheme;
    case net::ProxyServer::SCHEME_SOCKS4:
      return kSocks4Scheme;
    case net::ProxyServer::SCHEME_SOCKS5:
      return kSocks5Scheme;
    case net::ProxyServer::SCHEME_HTTPS:
      return url::kHttpsScheme;
    case net::ProxyServer::SCHEME_QUIC:
      return kQuicScheme;
    case net::ProxyServer::SCHEME_INVALID:
      break;
  }
  NOTREACHED();
  return "";
}

void SetProxyForScheme(const net::ProxyConfig::ProxyRules& proxy_rules,
                       const std::string& scheme,
                       const std::string& onc_scheme,
                       base::DictionaryValue* dict) {
  const net::ProxyList* proxy_list = nullptr;
  if (proxy_rules.type == net::ProxyConfig::ProxyRules::Type::PROXY_LIST) {
    proxy_list = &proxy_rules.single_proxies;
  } else if (proxy_rules.type ==
             net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME) {
    proxy_list = proxy_rules.MapUrlSchemeToProxyList(scheme);
  }
  if (!proxy_list || proxy_list->IsEmpty())
    return;
  const net::ProxyServer& server = proxy_list->Get();
  std::unique_ptr<base::DictionaryValue> url_dict(new base::DictionaryValue);
  std::string host = server.host_port_pair().host();

  // For all proxy types except SOCKS, the default scheme of the proxy host is
  // HTTP.
  net::ProxyServer::Scheme default_scheme =
      (onc_scheme == ::onc::proxy::kSocks) ? net::ProxyServer::SCHEME_SOCKS4
                                           : net::ProxyServer::SCHEME_HTTP;
  // Only prefix the host with a non-default scheme.
  if (server.scheme() != default_scheme)
    host = SchemeToString(server.scheme()) + "://" + host;
  url_dict->SetKey(::onc::proxy::kHost, base::Value(host));
  url_dict->SetKey(::onc::proxy::kPort,
                   base::Value(server.host_port_pair().port()));
  dict->SetWithoutPathExpansion(onc_scheme, std::move(url_dict));
}

// Returns the NetworkConfiugration with |guid| from |network_configs|, or
// nullptr if no such NetworkConfiguration is found.
const base::DictionaryValue* GetNetworkConfigByGUID(
    const base::ListValue& network_configs,
    const std::string& guid) {
  for (base::ListValue::const_iterator it = network_configs.begin();
       it != network_configs.end(); ++it) {
    const base::DictionaryValue* network = NULL;
    it->GetAsDictionary(&network);
    DCHECK(network);

    std::string current_guid;
    network->GetStringWithoutPathExpansion(::onc::network_config::kGUID,
                                           &current_guid);
    if (current_guid == guid)
      return network;
  }
  return NULL;
}

// Returns the first Ethernet NetworkConfiguration from |network_configs| with
// "Authentication: None", or nullptr if no such NetworkConfiguration is found.
const base::DictionaryValue* GetNetworkConfigForEthernetWithoutEAP(
    const base::ListValue& network_configs) {
  VLOG(2) << "Search for ethernet policy without EAP.";
  for (base::ListValue::const_iterator it = network_configs.begin();
       it != network_configs.end(); ++it) {
    const base::DictionaryValue* network = NULL;
    it->GetAsDictionary(&network);
    DCHECK(network);

    std::string type;
    network->GetStringWithoutPathExpansion(::onc::network_config::kType, &type);
    if (type != ::onc::network_type::kEthernet)
      continue;

    const base::DictionaryValue* ethernet = NULL;
    network->GetDictionaryWithoutPathExpansion(::onc::network_config::kEthernet,
                                               &ethernet);

    std::string auth;
    ethernet->GetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                            &auth);
    if (auth == ::onc::ethernet::kAuthenticationNone)
      return network;
  }
  return NULL;
}

// Returns the NetworkConfiguration object for |network| from
// |network_configs| or nullptr if no matching NetworkConfiguration is found. If
// |network| is a non-Ethernet network, performs a lookup by GUID. If |network|
// is an Ethernet network, tries lookup of the GUID of the shared EthernetEAP
// service, or otherwise returns the first Ethernet NetworkConfiguration with
// "Authentication: None".
const base::DictionaryValue* GetNetworkConfigForNetworkFromOnc(
    const base::ListValue& network_configs,
    const NetworkState& network) {
  // In all cases except Ethernet, we use the GUID of |network|.
  if (!network.Matches(NetworkTypePattern::Ethernet()))
    return GetNetworkConfigByGUID(network_configs, network.guid());

  // Ethernet is always shared and thus cannot store a GUID per user. Thus we
  // search for any Ethernet policy intead of a matching GUID.
  // EthernetEAP service contains only the EAP parameters and stores the GUID of
  // the respective ONC policy. The EthernetEAP service itself is however never
  // in state "connected". An EthernetEAP policy must be applied, if an Ethernet
  // service is connected using the EAP parameters.
  const NetworkState* ethernet_eap = nullptr;
  if (NetworkHandler::IsInitialized()) {
    ethernet_eap =
        NetworkHandler::Get()->network_state_handler()->GetEAPForEthernet(
            network.path(), /*connected_only=*/true);
  }

  // The GUID associated with the EthernetEAP service refers to the ONC policy
  // with "Authentication: 8021X".
  if (ethernet_eap)
    return GetNetworkConfigByGUID(network_configs, ethernet_eap->guid());

  // Otherwise, EAP is not used and instead the Ethernet policy with
  // "Authentication: None" applies.
  return GetNetworkConfigForEthernetWithoutEAP(network_configs);
}

// Expects |pref_name| in |pref_service| to be a pref holding an ONC blob.
// Returns the NetworkConfiguration ONC object for |network| from this ONC, or
// nullptr if no configuration is found. See |GetNetworkConfigForNetworkFromOnc|
// for the NetworkConfiguration lookup rules.
const base::DictionaryValue* GetPolicyForNetworkFromPref(
    const PrefService* pref_service,
    const char* pref_name,
    const NetworkState& network) {
  if (!pref_service) {
    VLOG(2) << "No pref service";
    return NULL;
  }

  const PrefService::Preference* preference =
      pref_service->FindPreference(pref_name);
  if (!preference) {
    VLOG(2) << "No preference " << pref_name;
    // The preference may not exist in tests.
    return NULL;
  }

  // User prefs are not stored in this Preference yet but only the policy.
  //
  // The policy server incorrectly configures the OpenNetworkConfiguration user
  // policy as Recommended. To work around that, we handle the Recommended and
  // the Mandatory value in the same way.
  // TODO(pneubeck): Remove this workaround, once the server is fixed. See
  // http://crbug.com/280553 .
  if (preference->IsDefaultValue()) {
    VLOG(2) << "Preference has no recommended or mandatory value.";
    // No policy set.
    return NULL;
  }
  VLOG(2) << "Preference with policy found.";
  const base::Value* onc_policy_value = preference->GetValue();
  DCHECK(onc_policy_value);

  const base::ListValue* onc_policy = NULL;
  onc_policy_value->GetAsList(&onc_policy);
  DCHECK(onc_policy);

  return GetNetworkConfigForNetworkFromOnc(*onc_policy, network);
}

// Returns the global network configuration dictionary from the ONC policy of
// the active user if |for_active_user| is true, or from device policy if it is
// false.
const base::DictionaryValue* GetGlobalConfigFromPolicy(bool for_active_user) {
  std::string username_hash;
  if (for_active_user) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (!user) {
      LOG(ERROR) << "No user logged in yet.";
      return NULL;
    }
    username_hash = user->username_hash();
  }
  return NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->GetGlobalConfigFromPolicy(username_hash);
}

}  // namespace

const char kEmptyUnencryptedConfiguration[] =
    "{\"Type\":\"UnencryptedConfiguration\",\"NetworkConfigurations\":[],"
    "\"Certificates\":[]}";

std::unique_ptr<base::Value> ReadDictionaryFromJson(const std::string& json) {
  if (json.empty()) {
    // Policy may contain empty values, just log a debug message.
    NET_LOG(DEBUG) << "Empty json string";
    return nullptr;
  }
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.value || !parsed_json.value->is_dict()) {
    NET_LOG(ERROR) << "Invalid JSON Dictionary: " << parsed_json.error_message;
    return nullptr;
  }
  return base::Value::ToUniquePtrValue(std::move(*parsed_json.value));
}

std::unique_ptr<base::Value> Decrypt(const std::string& passphrase,
                                     const base::Value& root) {
  const int kKeySizeInBits = 256;
  const int kMaxIterationCount = 500000;
  std::string onc_type;
  std::string initial_vector;
  std::string salt;
  std::string cipher;
  std::string stretch_method;
  std::string hmac_method;
  std::string hmac;
  int iterations;
  std::string ciphertext;

  if (!GetString(root, ::onc::encrypted::kCiphertext, &ciphertext) ||
      !GetString(root, ::onc::encrypted::kCipher, &cipher) ||
      !GetString(root, ::onc::encrypted::kHMAC, &hmac) ||
      !GetString(root, ::onc::encrypted::kHMACMethod, &hmac_method) ||
      !GetString(root, ::onc::encrypted::kIV, &initial_vector) ||
      !GetInt(root, ::onc::encrypted::kIterations, &iterations) ||
      !GetString(root, ::onc::encrypted::kSalt, &salt) ||
      !GetString(root, ::onc::encrypted::kStretch, &stretch_method) ||
      !GetString(root, ::onc::toplevel_config::kType, &onc_type) ||
      onc_type != ::onc::toplevel_config::kEncryptedConfiguration) {
    NET_LOG(ERROR) << "Encrypted ONC malformed.";
    return nullptr;
  }

  if (hmac_method != ::onc::encrypted::kSHA1 ||
      cipher != ::onc::encrypted::kAES256 ||
      stretch_method != ::onc::encrypted::kPBKDF2) {
    NET_LOG(ERROR) << "Encrypted ONC unsupported encryption scheme.";
    return nullptr;
  }

  // Make sure iterations != 0, since that's not valid.
  if (iterations == 0) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return nullptr;
  }

  // Simply a sanity check to make sure we can't lock up the machine
  // for too long with a huge number (or a negative number).
  if (iterations < 0 || iterations > kMaxIterationCount) {
    NET_LOG(ERROR) << "Too many iterations in encrypted ONC";
    return nullptr;
  }

  if (!base::Base64Decode(salt, &salt)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return nullptr;
  }

  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, passphrase, salt, iterations,
          kKeySizeInBits));

  if (!base::Base64Decode(initial_vector, &initial_vector)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return nullptr;
  }
  if (!base::Base64Decode(ciphertext, &ciphertext)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return nullptr;
  }
  if (!base::Base64Decode(hmac, &hmac)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return nullptr;
  }

  crypto::HMAC hmac_verifier(crypto::HMAC::SHA1);
  if (!hmac_verifier.Init(key.get()) ||
      !hmac_verifier.Verify(ciphertext, hmac)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return nullptr;
  }

  crypto::Encryptor decryptor;
  if (!decryptor.Init(key.get(), crypto::Encryptor::CBC, initial_vector)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return nullptr;
  }

  std::string plaintext;
  if (!decryptor.Decrypt(ciphertext, &plaintext)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return nullptr;
  }

  std::unique_ptr<base::Value> new_root = ReadDictionaryFromJson(plaintext);
  if (!new_root) {
    NET_LOG(ERROR) << "Property dictionary malformed.";
    return nullptr;
  }

  return new_root;
}

std::string GetSourceAsString(::onc::ONCSource source) {
  switch (source) {
    case ::onc::ONC_SOURCE_UNKNOWN:
      return "unknown";
    case ::onc::ONC_SOURCE_NONE:
      return "none";
    case ::onc::ONC_SOURCE_DEVICE_POLICY:
      return "device policy";
    case ::onc::ONC_SOURCE_USER_POLICY:
      return "user policy";
    case ::onc::ONC_SOURCE_USER_IMPORT:
      return "user import";
  }
  NOTREACHED();
  return "unknown";
}

void ExpandStringsInOncObject(const OncValueSignature& signature,
                              const VariableExpander& variable_expander,
                              base::DictionaryValue* onc_object) {
  if (&signature == &kEAPSignature) {
    ExpandField(::onc::eap::kAnonymousIdentity, variable_expander, onc_object);
    ExpandField(::onc::eap::kIdentity, variable_expander, onc_object);
  } else if (&signature == &kL2TPSignature ||
             &signature == &kOpenVPNSignature) {
    ExpandField(::onc::vpn::kUsername, variable_expander, onc_object);
  }

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(*onc_object); !it.IsAtEnd();
       it.Advance()) {
    base::DictionaryValue* inner_object = nullptr;
    if (!onc_object->GetDictionaryWithoutPathExpansion(it.key(), &inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());
    if (!field_signature)
      continue;

    ExpandStringsInOncObject(*field_signature->value_signature,
                             variable_expander, inner_object);
  }
}

void ExpandStringsInNetworks(const VariableExpander& variable_expander,
                             base::ListValue* network_configs) {
  for (auto& entry : *network_configs) {
    base::DictionaryValue* network = nullptr;
    entry.GetAsDictionary(&network);
    DCHECK(network);
    ExpandStringsInOncObject(kNetworkConfigurationSignature, variable_expander,
                             network);
  }
}

void FillInHexSSIDFieldsInOncObject(const OncValueSignature& signature,
                                    base::Value* onc_object) {
  DCHECK(onc_object->is_dict());
  if (&signature == &kWiFiSignature)
    FillInHexSSIDField(onc_object);

  // Recurse into nested objects.
  for (auto it : onc_object->DictItems()) {
    if (!it.second.is_dict())
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature)
      continue;

    FillInHexSSIDFieldsInOncObject(*field_signature->value_signature,
                                   &it.second);
  }
}

void FillInHexSSIDField(base::Value* wifi_fields) {
  if (wifi_fields->FindKey(::onc::wifi::kHexSSID))
    return;
  base::Value* ssid =
      wifi_fields->FindKeyOfType(::onc::wifi::kSSID, base::Value::Type::STRING);
  if (!ssid)
    return;
  std::string ssid_string = ssid->GetString();
  if (ssid_string.empty()) {
    NET_LOG(ERROR) << "Found empty SSID field.";
    return;
  }
  wifi_fields->SetKey(
      ::onc::wifi::kHexSSID,
      base::Value(base::HexEncode(ssid_string.c_str(), ssid_string.size())));
}

std::unique_ptr<base::DictionaryValue> MaskCredentialsInOncObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    const std::string& mask) {
  return OncMaskValues::Mask(signature, onc_object, mask);
}

std::string DecodePEM(const std::string& pem_encoded) {
  // The PEM block header used for DER certificates
  const char kCertificateHeader[] = "CERTIFICATE";

  // This is an older PEM marker for DER certificates.
  const char kX509CertificateHeader[] = "X509 CERTIFICATE";

  std::vector<std::string> pem_headers;
  pem_headers.push_back(kCertificateHeader);
  pem_headers.push_back(kX509CertificateHeader);

  net::PEMTokenizer pem_tokenizer(pem_encoded, pem_headers);
  std::string decoded;
  if (pem_tokenizer.GetNext()) {
    decoded = pem_tokenizer.data();
  } else {
    // If we failed to read the data as a PEM file, then try plain base64 decode
    // in case the PEM marker strings are missing. For this to work, there has
    // to be no white space, and it has to only contain the base64-encoded data.
    if (!base::Base64Decode(pem_encoded, &decoded)) {
      LOG(ERROR) << "Unable to base64 decode X509 data: " << pem_encoded;
      return std::string();
    }
  }
  return decoded;
}

bool ParseAndValidateOncForImport(const std::string& onc_blob,
                                  ::onc::ONCSource onc_source,
                                  const std::string& passphrase,
                                  base::ListValue* network_configs,
                                  base::DictionaryValue* global_network_config,
                                  base::ListValue* certificates) {
  if (network_configs)
    network_configs->Clear();
  if (global_network_config)
    global_network_config->Clear();
  if (certificates)
    certificates->Clear();
  if (onc_blob.empty())
    return true;

  std::unique_ptr<base::Value> toplevel_onc = ReadDictionaryFromJson(onc_blob);
  if (!toplevel_onc) {
    NET_LOG(ERROR) << "Not a valid ONC JSON dictionary: "
                   << GetSourceAsString(onc_source);
    return false;
  }

  // Check and see if this is an encrypted ONC file. If so, decrypt it.
  std::string onc_type;
  if (GetString(*toplevel_onc, ::onc::toplevel_config::kType, &onc_type) &&
      onc_type == ::onc::toplevel_config::kEncryptedConfiguration) {
    toplevel_onc = Decrypt(passphrase, *toplevel_onc);
    if (!toplevel_onc) {
      NET_LOG(ERROR) << "Unable to decrypt ONC from "
                     << GetSourceAsString(onc_source);
      return false;
    }
  }

  bool from_policy = (onc_source == ::onc::ONC_SOURCE_USER_POLICY ||
                      onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY);

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names and ignore invalid field names in kRecommended arrays.
  Validator validator(false,  // Ignore unknown fields.
                      false,  // Ignore invalid recommended field names.
                      true,   // Fail on missing fields.
                      from_policy,
                      true);  // Log warnings.
  validator.SetOncSource(onc_source);

  Validator::Result validation_result;
  std::unique_ptr<base::DictionaryValue> toplevel_onc_dict =
      base::DictionaryValue::From(std::move(toplevel_onc));
  toplevel_onc = validator.ValidateAndRepairObject(
      &kToplevelConfigurationSignature, *toplevel_onc_dict, &validation_result);

  if (from_policy) {
    UMA_HISTOGRAM_BOOLEAN("Enterprise.ONC.PolicyValidation",
                          validation_result == Validator::VALID);
  }

  bool success = true;
  if (validation_result == Validator::VALID_WITH_WARNINGS) {
    NET_LOG(DEBUG) << "ONC validation produced warnings: "
                   << GetSourceAsString(onc_source);
    success = false;
  } else if (validation_result == Validator::INVALID || !toplevel_onc) {
    NET_LOG(ERROR) << "ONC is invalid and couldn't be repaired: "
                   << GetSourceAsString(onc_source);
    return false;
  }

  if (certificates) {
    base::Value* validated_certs = toplevel_onc->FindKeyOfType(
        ::onc::toplevel_config::kCertificates, base::Value::Type::LIST);
    if (validated_certs)
      *certificates = base::ListValue(validated_certs->TakeList());
  }

  // Note that this processing is performed even if |network_configs| is
  // nullptr, because ResolveServerCertRefsInNetworks could affect the return
  // value of the function (which is supposed to aggregate validation issues in
  // all segments of the ONC blob).
  base::Value* validated_networks = toplevel_onc->FindKeyOfType(
      ::onc::toplevel_config::kNetworkConfigurations, base::Value::Type::LIST);
  base::ListValue* validated_networks_list;
  if (validated_networks &&
      validated_networks->GetAsList(&validated_networks_list)) {
    FillInHexSSIDFieldsInNetworks(validated_networks_list);

    CertPEMsByGUIDMap server_and_ca_certs =
        GetServerAndCACertsByGUID(*certificates);

    if (!ResolveServerCertRefsInNetworks(server_and_ca_certs,
                                         validated_networks_list)) {
      NET_LOG(ERROR) << "Some certificate references in the ONC policy could "
                        "not be resolved: "
                     << GetSourceAsString(onc_source);
      success = false;
    }

    if (network_configs)
      network_configs->Swap(validated_networks_list);
  }

  if (global_network_config) {
    base::Value* validated_global_config = toplevel_onc->FindKeyOfType(
        ::onc::toplevel_config::kGlobalNetworkConfiguration,
        base::Value::Type::DICTIONARY);
    if (validated_global_config) {
      base::DictionaryValue* validated_global_config_dict = nullptr;
      if (validated_global_config->GetAsDictionary(
              &validated_global_config_dict)) {
        global_network_config->Swap(validated_global_config_dict);
      }
    }
  }

  return success;
}

net::ScopedCERTCertificate DecodePEMCertificate(
    const std::string& pem_encoded) {
  std::string decoded = DecodePEM(pem_encoded);
  net::ScopedCERTCertificate cert =
      net::x509_util::CreateCERTCertificateFromBytes(
          reinterpret_cast<const uint8_t*>(decoded.data()), decoded.size());
  LOG_IF(ERROR, !cert.get())
      << "Couldn't create certificate from X509 data: " << decoded;
  return cert;
}

bool ResolveServerCertRefsInNetworks(const CertPEMsByGUIDMap& certs_by_guid,
                                     base::ListValue* network_configs) {
  bool success = true;
  for (base::ListValue::iterator it = network_configs->begin();
       it != network_configs->end();) {
    base::DictionaryValue* network = nullptr;
    it->GetAsDictionary(&network);
    if (!ResolveServerCertRefsInNetwork(certs_by_guid, network)) {
      std::string guid;
      network->GetStringWithoutPathExpansion(::onc::network_config::kGUID,
                                             &guid);
      // This might happen even with correct validation, if the referenced
      // certificate couldn't be imported.
      LOG(ERROR) << "Couldn't resolve some certificate reference of network "
                 << guid;
      it = network_configs->Erase(it, nullptr);
      success = false;
      continue;
    }
    ++it;
  }
  return success;
}

bool ResolveServerCertRefsInNetwork(const CertPEMsByGUIDMap& certs_by_guid,
                                    base::DictionaryValue* network_config) {
  return ResolveServerCertRefsInObject(
      certs_by_guid, kNetworkConfigurationSignature, network_config);
}

NetworkTypePattern NetworkTypePatternFromOncType(const std::string& type) {
  if (type == ::onc::network_type::kAllTypes)
    return NetworkTypePattern::Default();
  if (type == ::onc::network_type::kCellular)
    return NetworkTypePattern::Cellular();
  if (type == ::onc::network_type::kEthernet)
    return NetworkTypePattern::Ethernet();
  if (type == ::onc::network_type::kTether)
    return NetworkTypePattern::Tether();
  if (type == ::onc::network_type::kVPN)
    return NetworkTypePattern::VPN();
  if (type == ::onc::network_type::kWiFi)
    return NetworkTypePattern::WiFi();
  if (type == ::onc::network_type::kWireless)
    return NetworkTypePattern::Wireless();
  NET_LOG(ERROR) << "Unrecognized ONC type: " << type;
  return NetworkTypePattern::Default();
}

base::Value ConvertOncProxySettingsToProxyConfig(
    const base::Value& onc_proxy_settings) {
  std::string type = GetString(onc_proxy_settings, ::onc::proxy::kType);

  if (type == ::onc::proxy::kDirect) {
    return ProxyConfigDictionary::CreateDirect();
  }
  if (type == ::onc::proxy::kWPAD) {
    return ProxyConfigDictionary::CreateAutoDetect();
  }
  if (type == ::onc::proxy::kPAC) {
    std::string pac_url = GetString(onc_proxy_settings, ::onc::proxy::kPAC);
    GURL url(url_formatter::FixupURL(pac_url, std::string()));
    return ProxyConfigDictionary::CreatePacScript(
        url.is_valid() ? url.spec() : std::string(), false);
  }
  if (type == ::onc::proxy::kManual) {
    const base::Value* manual_dict =
        onc_proxy_settings.FindKey(::onc::proxy::kManual);
    if (!manual_dict) {
      NET_LOG(ERROR) << "Manual proxy missing dictionary";
      return base::Value();
    }
    std::string manual_spec;
    AppendProxyServerForScheme(*manual_dict, ::onc::proxy::kFtp, &manual_spec);
    AppendProxyServerForScheme(*manual_dict, ::onc::proxy::kHttp, &manual_spec);
    AppendProxyServerForScheme(*manual_dict, ::onc::proxy::kSocks,
                               &manual_spec);
    AppendProxyServerForScheme(*manual_dict, ::onc::proxy::kHttps,
                               &manual_spec);

    net::ProxyBypassRules bypass_rules;
    const base::Value* exclude_domains = onc_proxy_settings.FindKeyOfType(
        ::onc::proxy::kExcludeDomains, base::Value::Type::LIST);
    if (exclude_domains)
      bypass_rules = ConvertOncExcludeDomainsToBypassRules(*exclude_domains);
    return ProxyConfigDictionary::CreateFixedServers(manual_spec,
                                                     bypass_rules.ToString());
  }
  NOTREACHED();
  return base::Value();
}

base::Value ConvertProxyConfigToOncProxySettings(
    const base::Value& proxy_config_value) {
  // Create a ProxyConfigDictionary from the dictionary.
  ProxyConfigDictionary proxy_config(proxy_config_value.Clone());

  // Create the result DictionaryValue and populate it.
  base::Value proxy_settings(base::Value::Type::DICTIONARY);
  ProxyPrefs::ProxyMode mode;
  if (!proxy_config.GetMode(&mode))
    return base::Value();
  switch (mode) {
    case ProxyPrefs::MODE_DIRECT: {
      proxy_settings.SetKey(::onc::proxy::kType,
                            base::Value(::onc::proxy::kDirect));
      break;
    }
    case ProxyPrefs::MODE_AUTO_DETECT: {
      proxy_settings.SetKey(::onc::proxy::kType,
                            base::Value(::onc::proxy::kWPAD));
      break;
    }
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      proxy_settings.SetKey(::onc::proxy::kType,
                            base::Value(::onc::proxy::kPAC));
      std::string pac_url;
      proxy_config.GetPacUrl(&pac_url);
      proxy_settings.SetKey(::onc::proxy::kPAC, base::Value(pac_url));
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      proxy_settings.SetKey(::onc::proxy::kType,
                            base::Value(::onc::proxy::kManual));
      base::DictionaryValue manual;
      std::string proxy_rules_string;
      if (proxy_config.GetProxyServer(&proxy_rules_string)) {
        net::ProxyConfig::ProxyRules proxy_rules;
        proxy_rules.ParseFromString(proxy_rules_string);
        SetProxyForScheme(proxy_rules, url::kFtpScheme, ::onc::proxy::kFtp,
                          &manual);
        SetProxyForScheme(proxy_rules, url::kHttpScheme, ::onc::proxy::kHttp,
                          &manual);
        SetProxyForScheme(proxy_rules, url::kHttpsScheme, ::onc::proxy::kHttps,
                          &manual);
        SetProxyForScheme(proxy_rules, kSocksScheme, ::onc::proxy::kSocks,
                          &manual);
      }
      proxy_settings.SetKey(::onc::proxy::kManual, std::move(manual));

      // Convert the 'bypass_list' string into dictionary entries.
      std::string bypass_rules_string;
      if (proxy_config.GetBypassList(&bypass_rules_string)) {
        net::ProxyBypassRules bypass_rules;
        bypass_rules.ParseFromString(bypass_rules_string);
        base::ListValue exclude_domains;
        for (const auto& rule : bypass_rules.rules())
          exclude_domains.AppendString(rule->ToString());
        if (!exclude_domains.empty()) {
          proxy_settings.SetKey(::onc::proxy::kExcludeDomains,
                                std::move(exclude_domains));
        }
      }
      break;
    }
    default: {
      LOG(ERROR) << "Unexpected proxy mode in Shill config: " << mode;
      return base::Value();
    }
  }
  return proxy_settings;
}

void ExpandStringPlaceholdersInNetworksForUser(
    const user_manager::User* user,
    base::ListValue* network_configs) {
  if (!user) {
    // In tests no user may be logged in. It's not harmful if we just don't
    // expand the strings.
    return;
  }

  // Note: It is OK for the placeholders to be replaced with empty strings if
  // that is what the getters on |user| provide.
  std::map<std::string, std::string> substitutions;
  substitutions[::onc::substitutes::kLoginID] = user->GetAccountName(false);
  substitutions[::onc::substitutes::kLoginEmail] =
      user->GetAccountId().GetUserEmail();
  VariableExpander variable_expander(std::move(substitutions));
  chromeos::onc::ExpandStringsInNetworks(variable_expander, network_configs);
}

int ImportNetworksForUser(const user_manager::User* user,
                          const base::ListValue& network_configs,
                          std::string* error) {
  error->clear();

  std::unique_ptr<base::ListValue> expanded_networks(
      network_configs.DeepCopy());
  ExpandStringPlaceholdersInNetworksForUser(user, expanded_networks.get());

  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetProfileForUserhash(
          user->username_hash());
  if (!profile) {
    *error = "User profile doesn't exist for: " + user->display_email();
    return 0;
  }

  bool ethernet_not_found = false;
  int networks_created = 0;
  for (base::ListValue::const_iterator it = expanded_networks->begin();
       it != expanded_networks->end(); ++it) {
    const base::DictionaryValue* network = NULL;
    it->GetAsDictionary(&network);
    DCHECK(network);

    // Remove irrelevant fields.
    onc::Normalizer normalizer(true /* remove recommended fields */);
    std::unique_ptr<base::DictionaryValue> normalized_network =
        normalizer.NormalizeObject(&onc::kNetworkConfigurationSignature,
                                   *network);

    // TODO(pneubeck): Use ONC and ManagedNetworkConfigurationHandler instead.
    // crbug.com/457936
    std::unique_ptr<base::DictionaryValue> shill_dict =
        onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                       *normalized_network);

    std::unique_ptr<NetworkUIData> ui_data(
        NetworkUIData::CreateFromONC(::onc::ONC_SOURCE_USER_IMPORT));
    shill_dict->SetKey(shill::kUIDataProperty,
                       base::Value(ui_data->GetAsJson()));
    shill_dict->SetKey(shill::kProfileProperty, base::Value(profile->path));

    std::string type;
    shill_dict->GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
    NetworkConfigurationHandler* config_handler =
        NetworkHandler::Get()->network_configuration_handler();
    if (NetworkTypePattern::Ethernet().MatchesType(type)) {
      // Ethernet has to be configured using an existing Ethernet service.
      const NetworkState* ethernet =
          NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
              NetworkTypePattern::Ethernet());
      if (ethernet) {
        config_handler->SetShillProperties(ethernet->path(), *shill_dict,
                                           base::OnceClosure(),
                                           network_handler::ErrorCallback());
      } else {
        ethernet_not_found = true;
      }

    } else {
      config_handler->CreateShillConfiguration(
          *shill_dict, network_handler::ServiceResultCallback(),
          network_handler::ErrorCallback());
      ++networks_created;
    }
  }

  if (ethernet_not_found)
    *error = "No Ethernet available to configure.";
  return networks_created;
}

const base::DictionaryValue* FindPolicyForActiveUser(
    const std::string& guid,
    ::onc::ONCSource* onc_source) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  std::string username_hash = user ? user->username_hash() : std::string();
  return NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->FindPolicyByGUID(username_hash, guid, onc_source);
}

bool PolicyAllowsOnlyPolicyNetworksToAutoconnect(bool for_active_user) {
  const base::DictionaryValue* global_config =
      GetGlobalConfigFromPolicy(for_active_user);
  if (!global_config)
    return false;  // By default, all networks are allowed to autoconnect.

  bool only_policy_autoconnect = false;
  global_config->GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      &only_policy_autoconnect);
  return only_policy_autoconnect;
}

const base::DictionaryValue* GetPolicyForNetwork(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs,
    const NetworkState& network,
    ::onc::ONCSource* onc_source) {
  VLOG(2) << "GetPolicyForNetwork: " << network.path();
  *onc_source = ::onc::ONC_SOURCE_NONE;

  const base::DictionaryValue* network_policy = GetPolicyForNetworkFromPref(
      profile_prefs, ::onc::prefs::kOpenNetworkConfiguration, network);
  if (network_policy) {
    VLOG(1) << "Network " << network.path() << " is managed by user policy.";
    *onc_source = ::onc::ONC_SOURCE_USER_POLICY;
    return network_policy;
  }
  network_policy = GetPolicyForNetworkFromPref(
      local_state_prefs, ::onc::prefs::kDeviceOpenNetworkConfiguration,
      network);
  if (network_policy) {
    VLOG(1) << "Network " << network.path() << " is managed by device policy.";
    *onc_source = ::onc::ONC_SOURCE_DEVICE_POLICY;
    return network_policy;
  }
  VLOG(2) << "Network " << network.path() << " is unmanaged.";
  return NULL;
}

bool HasPolicyForNetwork(const PrefService* profile_prefs,
                         const PrefService* local_state_prefs,
                         const NetworkState& network) {
  ::onc::ONCSource ignored_onc_source;
  const base::DictionaryValue* policy = onc::GetPolicyForNetwork(
      profile_prefs, local_state_prefs, network, &ignored_onc_source);
  return policy != NULL;
}

bool HasUserPasswordSubsitutionVariable(const OncValueSignature& signature,
                                        base::DictionaryValue* onc_object) {
  if (&signature == &kEAPSignature) {
    std::string password_field;
    if (!onc_object->GetStringWithoutPathExpansion(::onc::eap::kPassword,
                                                   &password_field)) {
      return false;
    }

    if (password_field == ::onc::substitutes::kPasswordPlaceholderVerbatim) {
      return true;
    }
  }

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(*onc_object); !it.IsAtEnd();
       it.Advance()) {
    base::DictionaryValue* inner_object = nullptr;
    if (!onc_object->GetDictionaryWithoutPathExpansion(it.key(), &inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());
    if (!field_signature)
      continue;

    bool result = HasUserPasswordSubsitutionVariable(
        *field_signature->value_signature, inner_object);
    if (result) {
      return true;
    }
  }

  return false;
}

bool HasUserPasswordSubsitutionVariable(base::ListValue* network_configs) {
  for (auto& entry : *network_configs) {
    base::DictionaryValue* network = nullptr;
    entry.GetAsDictionary(&network);
    DCHECK(network);

    bool result = HasUserPasswordSubsitutionVariable(
        kNetworkConfigurationSignature, network);
    if (result) {
      return true;
    }
  }
  return false;
}

}  // namespace onc
}  // namespace chromeos
