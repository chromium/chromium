// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The implementation of TranslateONCObjectToShill is structured in two parts:
// - The recursion through the existing ONC hierarchy
//     see TranslateONCHierarchy
// - The local translation of an object depending on the associated signature
//     see LocalTranslator::TranslateFields

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "net/base/ip_address.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

namespace {

std::unique_ptr<base::Value> ConvertValueToString(const base::Value& value) {
  std::string str;
  if (!value.GetAsString(&str))
    base::JSONWriter::Write(value, &str);
  return std::make_unique<base::Value>(str);
}

// Sets any client cert properties when ClientCertType is PKCS11Id.
void SetClientCertProperties(client_cert::ConfigType config_type,
                             const base::DictionaryValue* onc_object,
                             base::DictionaryValue* shill_dictionary) {
  std::string cert_type;
  onc_object->GetStringWithoutPathExpansion(::onc::client_cert::kClientCertType,
                                            &cert_type);
  if (cert_type != ::onc::client_cert::kPKCS11Id)
    return;

  std::string pkcs11_id;
  onc_object->GetStringWithoutPathExpansion(
      ::onc::client_cert::kClientCertPKCS11Id, &pkcs11_id);
  if (pkcs11_id.empty()) {
    // If the cert type is PKCS11 but the pkcs11 id is empty, set empty
    // properties to indicate 'no certificate'.
    client_cert::SetEmptyShillProperties(config_type, shill_dictionary);
    return;
  }

  int slot_id;
  std::string cert_id =
      client_cert::GetPkcs11AndSlotIdFromEapCertId(pkcs11_id, &slot_id);
  client_cert::SetShillProperties(config_type, slot_id, cert_id,
                                  shill_dictionary);
}

// This class is responsible to translate the local fields of the given
// |onc_object| according to |onc_signature| into |shill_dictionary|. This
// translation should consider (if possible) only fields of this ONC object and
// not nested objects because recursion is handled by the calling function
// TranslateONCHierarchy.
class LocalTranslator {
 public:
  LocalTranslator(const OncValueSignature& onc_signature,
                  const base::DictionaryValue& onc_object,
                  base::DictionaryValue* shill_dictionary)
      : onc_signature_(&onc_signature),
        onc_object_(&onc_object),
        shill_dictionary_(shill_dictionary) {
    field_translation_table_ = GetFieldTranslationTable(onc_signature);
  }

  void TranslateFields();

 private:
  void TranslateEthernet();
  void TranslateOpenVPN();
  void TranslateIPsec();
  void TranslateL2TP();
  void TranslateVPN();
  void TranslateWiFi();
  void TranslateEAP();
  void TranslateStaticIPConfig();
  void TranslateNetworkConfiguration();

  // Copies all entries from |onc_object_| to |shill_dictionary_| for which a
  // translation (shill_property_name) is defined by the translation table for
  // |onc_signature_|.
  void CopyFieldsAccordingToSignature();

  // If existent, copies the value of field |onc_field_name| from |onc_object_|
  // to the property |shill_property_name| in |shill_dictionary_|.
  void CopyFieldFromONCToShill(const std::string& onc_field_name,
                               const std::string& shill_property_name);

  // Adds |value| to |shill_dictionary| at the field shill_property_name given
  // by the associated signature. Takes ownership of |value|. Does nothing if
  // |value| is NULL or the property name cannot be read from the signature.
  void AddValueAccordingToSignature(const std::string& onc_field_name,
                                    std::unique_ptr<base::Value> value);

  // Translates the value |onc_value| using |table|. It is an error if no
  // matching table entry is found. Writes the result as entry at
  // |shill_property_name| in |shill_dictionary_|.
  void TranslateWithTableAndSet(const std::string& onc_value,
                                const StringTranslationEntry table[],
                                const std::string& shill_property_name);

  const OncValueSignature* onc_signature_;
  const FieldTranslationEntry* field_translation_table_;
  const base::DictionaryValue* onc_object_;
  base::DictionaryValue* shill_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(LocalTranslator);
};

void LocalTranslator::TranslateFields() {
  if (onc_signature_ == &kNetworkConfigurationSignature)
    TranslateNetworkConfiguration();
  else if (onc_signature_ == &kEthernetSignature)
    TranslateEthernet();
  else if (onc_signature_ == &kVPNSignature)
    TranslateVPN();
  else if (onc_signature_ == &kOpenVPNSignature)
    TranslateOpenVPN();
  else if (onc_signature_ == &kIPsecSignature)
    TranslateIPsec();
  else if (onc_signature_ == &kL2TPSignature)
    TranslateL2TP();
  else if (onc_signature_ == &kWiFiSignature)
    TranslateWiFi();
  else if (onc_signature_ == &kEAPSignature)
    TranslateEAP();
  else if (onc_signature_ == &kStaticIPConfigSignature)
    TranslateStaticIPConfig();
  else
    CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateEthernet() {
  std::string authentication;
  onc_object_->GetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                             &authentication);

  const char* shill_type = shill::kTypeEthernet;
  if (authentication == ::onc::ethernet::k8021X)
    shill_type = shill::kTypeEthernetEap;
  shill_dictionary_->SetKey(shill::kTypeProperty, base::Value(shill_type));

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateOpenVPN() {
  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  CopyFieldFromONCToShill(::onc::vpn::kSaveCredentials,
                          shill::kSaveCredentialsProperty);

  std::string user_auth_type;
  onc_object_->GetStringWithoutPathExpansion(
      ::onc::openvpn::kUserAuthenticationType, &user_auth_type);
  // The default behavior (if user_auth_type is empty) is to use both password
  // and OTP in a static challenge and only the password otherwise. As long as
  // Shill doe not know about the exact user authentication type, this is
  // identical to kPasswordAndOTP.
  if (user_auth_type.empty())
    user_auth_type = ::onc::openvpn_user_auth_type::kPasswordAndOTP;
  NET_LOG(DEBUG) << "USER AUTH TYPE: " << user_auth_type;
  if (user_auth_type == ::onc::openvpn_user_auth_type::kPassword ||
      user_auth_type == ::onc::openvpn_user_auth_type::kPasswordAndOTP) {
    CopyFieldFromONCToShill(::onc::openvpn::kPassword,
                            shill::kOpenVPNPasswordProperty);
  }
  if (user_auth_type == ::onc::openvpn_user_auth_type::kPasswordAndOTP)
    CopyFieldFromONCToShill(::onc::openvpn::kOTP, shill::kOpenVPNOTPProperty);
  if (user_auth_type == ::onc::openvpn_user_auth_type::kOTP)
    CopyFieldFromONCToShill(::onc::openvpn::kOTP, shill::kOpenVPNTokenProperty);

  // Shill supports only one RemoteCertKU but ONC specifies a list, so copy only
  // the first entry if the lists exists. Otherwise copy an empty string to
  // reset any previous configuration.
  const base::ListValue* cert_kus = nullptr;
  std::string cert_ku;
  if (onc_object_->GetListWithoutPathExpansion(::onc::openvpn::kRemoteCertKU,
                                               &cert_kus)) {
    cert_kus->GetString(0, &cert_ku);
  }
  shill_dictionary_->SetKey(shill::kOpenVPNRemoteCertKUProperty,
                            base::Value(cert_ku));

  SetClientCertProperties(client_cert::CONFIG_TYPE_OPENVPN, onc_object_,
                          shill_dictionary_);

  // Modified CopyFieldsAccordingToSignature to handle RemoteCertKU and
  // ServerCAPEMs and handle all other fields as strings.
  for (base::DictionaryValue::Iterator it(*onc_object_); !it.IsAtEnd();
       it.Advance()) {
    std::string key = it.key();
    std::unique_ptr<base::Value> translated;
    if (key == ::onc::openvpn::kRemoteCertKU ||
        key == ::onc::openvpn::kServerCAPEMs ||
        key == ::onc::openvpn::kExtraHosts) {
      translated.reset(it.value().DeepCopy());
    } else {
      // Shill wants all Provider/VPN fields to be strings.
      translated = ConvertValueToString(it.value());
    }
    AddValueAccordingToSignature(key, std::move(translated));
  }
}

void LocalTranslator::TranslateIPsec() {
  SetClientCertProperties(client_cert::CONFIG_TYPE_IPSEC, onc_object_,
                          shill_dictionary_);

  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  CopyFieldFromONCToShill(::onc::vpn::kSaveCredentials,
                          shill::kSaveCredentialsProperty);

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateL2TP() {
  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  CopyFieldFromONCToShill(::onc::vpn::kSaveCredentials,
                          shill::kSaveCredentialsProperty);

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateVPN() {
  std::string onc_type;
  if (onc_object_->GetStringWithoutPathExpansion(::onc::vpn::kType,
                                                 &onc_type)) {
    TranslateWithTableAndSet(onc_type, kVPNTypeTable,
                             shill::kProviderTypeProperty);
  }
  if (onc_type == ::onc::vpn::kThirdPartyVpn) {
    // For third-party VPNs, |shill::kProviderHostProperty| is used to store the
    // provider's extension ID.
    const base::DictionaryValue* onc_third_party_vpn = nullptr;
    onc_object_->GetDictionaryWithoutPathExpansion(::onc::vpn::kThirdPartyVpn,
                                                   &onc_third_party_vpn);
    std::string onc_extension_id;
    if (onc_third_party_vpn &&
        onc_third_party_vpn->GetStringWithoutPathExpansion(
            ::onc::third_party_vpn::kExtensionID, &onc_extension_id)) {
      shill_dictionary_->SetKey(shill::kProviderHostProperty,
                                base::Value(onc_extension_id));
    }
  } else {
    CopyFieldFromONCToShill(::onc::vpn::kHost, shill::kProviderHostProperty);
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateWiFi() {
  std::string security;
  if (onc_object_->GetStringWithoutPathExpansion(::onc::wifi::kSecurity,
                                                 &security)) {
    TranslateWithTableAndSet(security, kWiFiSecurityTable,
                             shill::kSecurityClassProperty);
    if (security == ::onc::wifi::kWEP_8021X) {
      shill_dictionary_->SetKey(shill::kEapKeyMgmtProperty,
                                base::Value(shill::kKeyManagementIEEE8021X));
    }
  }

  // We currently only support managed and no adhoc networks.
  shill_dictionary_->SetKey(shill::kModeProperty,
                            base::Value(shill::kModeManaged));

  bool allow_gateway_arp_polling;
  if (onc_object_->GetBooleanWithoutPathExpansion(
          ::onc::wifi::kAllowGatewayARPPolling, &allow_gateway_arp_polling)) {
    shill_dictionary_->SetKey(shill::kLinkMonitorDisableProperty,
                              base::Value(!allow_gateway_arp_polling));
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateEAP() {
  std::string outer;
  onc_object_->GetStringWithoutPathExpansion(::onc::eap::kOuter, &outer);
  if (!outer.empty())
    TranslateWithTableAndSet(outer, kEAPOuterTable, shill::kEapMethodProperty);

  // Translate the inner protocol only for outer tunneling protocols.
  if (outer == ::onc::eap::kPEAP || outer == ::onc::eap::kEAP_TTLS) {
    // In ONC the Inner protocol defaults to "Automatic".
    std::string inner = ::onc::eap::kAutomatic;
    // ONC's Inner == "Automatic" translates to omitting the Phase2 property in
    // Shill.
    onc_object_->GetStringWithoutPathExpansion(::onc::eap::kInner, &inner);
    if (inner != ::onc::eap::kAutomatic) {
      const StringTranslationEntry* table = outer == ::onc::eap::kPEAP
                                                ? kEAP_PEAP_InnerTable
                                                : kEAP_TTLS_InnerTable;
      TranslateWithTableAndSet(inner, table, shill::kEapPhase2AuthProperty);
    }
  }

  SetClientCertProperties(client_cert::CONFIG_TYPE_EAP, onc_object_,
                          shill_dictionary_);

  // Set shill::kEapUseLoginPasswordProperty according to whether or not the
  // password substitution variable is set.
  const base::Value* password_field =
      onc_object_->FindKey(::onc::eap::kPassword);
  if (password_field && password_field->GetString() ==
                            ::onc::substitutes::kPasswordPlaceholderVerbatim) {
    shill_dictionary_->SetKey(shill::kEapUseLoginPasswordProperty,
                              base::Value(true));
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateStaticIPConfig() {
  CopyFieldsAccordingToSignature();
  // Shill expects 4 valid nameserver values. Ensure all values are valid and
  // replace any invalid values with 0.0.0.0 (which has no effect). See
  // https://crbug.com/922219 for details.
  base::Value* name_servers =
      shill_dictionary_->FindKey(shill::kNameServersProperty);
  if (name_servers) {
    static const char kDefaultIpAddr[] = "0.0.0.0";
    net::IPAddress ip_addr;
    for (base::Value& value_ref : name_servers->GetList()) {
      // AssignFromIPLiteral returns true if a string is valid ipv4 or ipv6.
      if (!ip_addr.AssignFromIPLiteral(value_ref.GetString()))
        value_ref = base::Value(kDefaultIpAddr);
    }
    while (name_servers->GetList().size() < 4)
      name_servers->Append(base::Value(kDefaultIpAddr));
  }
}

void LocalTranslator::TranslateNetworkConfiguration() {
  std::string type;
  onc_object_->GetStringWithoutPathExpansion(::onc::network_config::kType,
                                             &type);

  if (type == ::onc::network_type::kWimaxDeprecated) {
    NET_LOG(ERROR) << "WiMAX ONC configuration is no longer supported.";
    return;
  }

  // Note; The Ethernet type might be overridden to EthernetEap in
  // TranslateEthernet if Ethernet specific properties are provided.
  TranslateWithTableAndSet(type, kNetworkTypeTable, shill::kTypeProperty);

  // Shill doesn't allow setting the name for non-VPN networks.
  if (type == ::onc::network_type::kVPN)
    CopyFieldFromONCToShill(::onc::network_config::kName, shill::kNameProperty);

  std::string ip_address_config_type, name_servers_config_type;
  onc_object_->GetStringWithoutPathExpansion(
      ::onc::network_config::kIPAddressConfigType, &ip_address_config_type);
  onc_object_->GetStringWithoutPathExpansion(
      ::onc::network_config::kNameServersConfigType, &name_servers_config_type);
  if ((ip_address_config_type == ::onc::network_config::kIPConfigTypeDHCP) ||
      (name_servers_config_type == ::onc::network_config::kIPConfigTypeDHCP)) {
    // If either type is set to DHCP, provide an empty dictionary to ensure
    // that any unset properties are cleared. Note: if either type is specified,
    // the other type defaults to DHCP if not specified.
    shill_dictionary_->SetWithoutPathExpansion(
        shill::kStaticIPConfigProperty,
        std::make_unique<base::DictionaryValue>());
  }

  const base::DictionaryValue* proxy_settings = nullptr;
  if (onc_object_->GetDictionaryWithoutPathExpansion(
          ::onc::network_config::kProxySettings, &proxy_settings)) {
    base::Value proxy_config =
        ConvertOncProxySettingsToProxyConfig(*proxy_settings);
    std::string proxy_config_str;
    base::JSONWriter::Write(proxy_config, &proxy_config_str);
    shill_dictionary_->SetKey(shill::kProxyConfigProperty,
                              base::Value(proxy_config_str));
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::CopyFieldsAccordingToSignature() {
  for (base::DictionaryValue::Iterator it(*onc_object_); !it.IsAtEnd();
       it.Advance()) {
    AddValueAccordingToSignature(it.key(),
                                 base::WrapUnique(it.value().DeepCopy()));
  }
}

void LocalTranslator::CopyFieldFromONCToShill(
    const std::string& onc_field_name,
    const std::string& shill_property_name) {
  const base::Value* value = NULL;
  if (!onc_object_->GetWithoutPathExpansion(onc_field_name, &value))
    return;

  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  if (field_signature) {
    base::Value::Type expected_type =
        field_signature->value_signature->onc_type;
    if (value->type() != expected_type) {
      NET_LOG(ERROR) << "Found field " << onc_field_name << " of type "
                     << value->type() << " but expected type " << expected_type;
      return;
    }
  } else {
    NET_LOG(ERROR)
        << "Attempt to translate a field that is not part of the ONC format.";
    return;
  }
  shill_dictionary_->SetKey(shill_property_name, value->Clone());
}

void LocalTranslator::AddValueAccordingToSignature(
    const std::string& onc_name,
    std::unique_ptr<base::Value> value) {
  if (!value || !field_translation_table_)
    return;
  std::string shill_property_name;
  if (!GetShillPropertyName(onc_name, field_translation_table_,
                            &shill_property_name)) {
    return;
  }

  shill_dictionary_->SetWithoutPathExpansion(shill_property_name,
                                             std::move(value));
}

void LocalTranslator::TranslateWithTableAndSet(
    const std::string& onc_value,
    const StringTranslationEntry table[],
    const std::string& shill_property_name) {
  std::string shill_value;
  if (TranslateStringToShill(table, onc_value, &shill_value)) {
    shill_dictionary_->SetKey(shill_property_name, base::Value(shill_value));
    return;
  }
  // As we previously validate ONC, this case should never occur. If it still
  // occurs, we should check here. Otherwise the failure will only show up much
  // later in Shill.
  NET_LOG(ERROR) << "Value '" << onc_value
                 << "' cannot be translated to Shill property: "
                 << shill_property_name;
}

// Iterates recursively over |onc_object| and its |signature|. At each object
// applies the local translation using LocalTranslator::TranslateFields. The
// results are written to |shill_dictionary|.
void TranslateONCHierarchy(const OncValueSignature& signature,
                           const base::DictionaryValue& onc_object,
                           base::Value* shill_dictionary) {
  const std::vector<std::string> path =
      GetPathToNestedShillDictionary(signature);
  const std::vector<base::StringPiece> path_pieces(path.begin(), path.end());
  base::Value* target_shill_dictionary = shill_dictionary->FindPathOfType(
      path_pieces, base::Value::Type::DICTIONARY);
  if (!target_shill_dictionary) {
    target_shill_dictionary = shill_dictionary->SetPath(
        path_pieces, base::Value(base::Value::Type::DICTIONARY));
  }

  // Translates fields of |onc_object| and writes them to
  // |target_shill_dictionary_| nested in |shill_dictionary|.
  LocalTranslator translator(
      signature, onc_object,
      static_cast<base::DictionaryValue*>(target_shill_dictionary));
  translator.TranslateFields();

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(onc_object); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* inner_object = NULL;
    if (!it.value().GetAsDictionary(&inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());
    if (!field_signature) {
      NET_LOG(ERROR) << "Unexpected or deprecated ONC key: " << it.key();
      continue;
    }
    TranslateONCHierarchy(*field_signature->value_signature, *inner_object,
                          shill_dictionary);
  }
}

}  // namespace

std::unique_ptr<base::DictionaryValue> TranslateONCObjectToShill(
    const OncValueSignature* onc_signature,
    const base::DictionaryValue& onc_object) {
  CHECK(onc_signature != NULL);
  std::unique_ptr<base::DictionaryValue> shill_dictionary(
      new base::DictionaryValue);
  TranslateONCHierarchy(*onc_signature, onc_object, shill_dictionary.get());
  return shill_dictionary;
}

}  // namespace onc
}  // namespace chromeos
