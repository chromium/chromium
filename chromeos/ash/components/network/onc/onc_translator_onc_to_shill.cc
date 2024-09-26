// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The implementation of TranslateONCObjectToShill is structured in two parts:
// - The recursion through the existing ONC hierarchy
//     see TranslateONCHierarchy
// - The local translation of an object depending on the associated signature
//     see LocalTranslator::TranslateFields

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/onc/onc_translation_tables.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "components/onc/onc_constants.h"
#include "net/base/ip_address.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::onc {

namespace {

// Converts values to JSON strings. This will turn booleans into "true" or
// "false" which is what Shill expects for VPN values (including L2TP).
base::Value ConvertVpnValueToString(const base::Value& value) {
  if (value.is_string())
    return value.Clone();
  std::string str;
  CHECK(base::JSONWriter::Write(value, &str));
  return base::Value(str);
}

// Returns the string value of |key| from |dict| if found, or the empty string
// otherwise.
std::string FindStringKeyOrEmpty(const base::Value::Dict& dict,
                                 std::string_view key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

// Sets any client cert properties when ClientCertType is PKCS11Id.
void SetClientCertProperties(client_cert::ConfigType config_type,
                             const base::Value::Dict& onc_object,
                             base::Value::Dict* shill_dictionary) {
  const std::string cert_type =
      FindStringKeyOrEmpty(onc_object, ::onc::client_cert::kClientCertType);
  if (cert_type != ::onc::client_cert::kPKCS11Id)
    return;

  const std::string pkcs11_id =
      FindStringKeyOrEmpty(onc_object, ::onc::client_cert::kClientCertPKCS11Id);
  if (pkcs11_id.empty()) {
    // If the cert type is PKCS11 but the pkcs11 id is empty, set empty
    // properties to indicate 'no certificate'.
    client_cert::SetEmptyShillProperties(config_type, *shill_dictionary);
    return;
  }

  int slot_id;
  std::string cert_id =
      client_cert::GetPkcs11AndSlotIdFromEapCertId(pkcs11_id, &slot_id);
  client_cert::SetShillProperties(config_type, slot_id, cert_id,
                                  *shill_dictionary);
}

// This class is responsible to translate the local fields of the given
// |onc_object| according to |onc_signature| into |shill_dictionary|. This
// translation should consider (if possible) only fields of this ONC object and
// not nested objects because recursion is handled by the calling function
// TranslateONCHierarchy.
class LocalTranslator {
 public:
  LocalTranslator(const chromeos::onc::OncValueSignature& onc_signature,
                  const base::Value::Dict& onc_object,
                  base::Value::Dict* shill_dictionary)
      : onc_signature_(&onc_signature),
        onc_object_(&onc_object),
        shill_dictionary_(shill_dictionary) {
    field_translation_table_ = GetFieldTranslationTable(onc_signature);
  }

  LocalTranslator(const LocalTranslator&) = delete;
  LocalTranslator& operator=(const LocalTranslator&) = delete;

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
  void TranslateCellular();
  void TranslateApn();

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
  // |value| is none or the property name cannot be read from the signature.
  void AddValueAccordingToSignature(const std::string& onc_field_name,
                                    const base::Value& value);

  // Translates the value |onc_value| using |table|. It is an error if no
  // matching table entry is found. Writes the result as entry at
  // |shill_property_name| in |shill_dictionary_|.
  void TranslateWithTableAndSet(const std::string& onc_value,
                                const StringTranslationEntry table[],
                                const std::string& shill_property_name);

  raw_ptr<const chromeos::onc::OncValueSignature> onc_signature_;
  raw_ptr<const FieldTranslationEntry> field_translation_table_;
  raw_ptr<const base::Value::Dict> onc_object_;
  raw_ptr<base::Value::Dict> shill_dictionary_;
};

void LocalTranslator::TranslateFields() {
  if (onc_signature_ == &chromeos::onc::kNetworkConfigurationSignature) {
    TranslateNetworkConfiguration();
  } else if (onc_signature_ == &chromeos::onc::kCellularSignature) {
    TranslateCellular();
  } else if (onc_signature_ == &chromeos::onc::kCellularApnSignature) {
    TranslateApn();
  } else if (onc_signature_ == &chromeos::onc::kEthernetSignature) {
    TranslateEthernet();
  } else if (onc_signature_ == &chromeos::onc::kVPNSignature) {
    TranslateVPN();
  } else if (onc_signature_ == &chromeos::onc::kOpenVPNSignature) {
    TranslateOpenVPN();
  } else if (onc_signature_ == &chromeos::onc::kIPsecSignature) {
    TranslateIPsec();
  } else if (onc_signature_ == &chromeos::onc::kL2TPSignature) {
    TranslateL2TP();
  } else if (onc_signature_ == &chromeos::onc::kWiFiSignature) {
    TranslateWiFi();
  } else if (onc_signature_ == &chromeos::onc::kEAPSignature) {
    TranslateEAP();
  } else if (onc_signature_ == &chromeos::onc::kStaticIPConfigSignature) {
    TranslateStaticIPConfig();
  } else {
    CopyFieldsAccordingToSignature();
  }
}

void LocalTranslator::TranslateEthernet() {
  const std::string* authentication =
      onc_object_->FindString(::onc::ethernet::kAuthentication);

  const char* shill_type = shill::kTypeEthernet;
  if (authentication && *authentication == ::onc::ethernet::k8021X)
    shill_type = shill::kTypeEthernetEap;
  shill_dictionary_->Set(shill::kTypeProperty, shill_type);

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateOpenVPN() {
  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  CopyFieldFromONCToShill(::onc::vpn::kSaveCredentials,
                          shill::kSaveCredentialsProperty);

  std::string user_auth_type = FindStringKeyOrEmpty(
      *onc_object_, ::onc::openvpn::kUserAuthenticationType);
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
  const base::Value::List* cert_kus =
      onc_object_->FindList(::onc::openvpn::kRemoteCertKU);
  std::string cert_ku;
  if (cert_kus) {
    if (!cert_kus->empty() && (*cert_kus)[0].is_string()) {
      cert_ku = (*cert_kus)[0].GetString();
    }
  }
  shill_dictionary_->Set(shill::kOpenVPNRemoteCertKUProperty, cert_ku);

  SetClientCertProperties(client_cert::ConfigType::kOpenVpn, *onc_object_,
                          shill_dictionary_);

  const std::string* compression_algorithm =
      onc_object_->FindString(::onc::openvpn::kCompressionAlgorithm);
  if (compression_algorithm) {
    TranslateWithTableAndSet(*compression_algorithm,
                             kOpenVpnCompressionAlgorithmTable,
                             shill::kOpenVPNCompressProperty);
  }

  // Modified CopyFieldsAccordingToSignature to handle RemoteCertKU and
  // ServerCAPEMs and handle all other fields as strings.
  for (const auto it : *onc_object_) {
    std::string key = it.first;
    base::Value translated;
    if (key == ::onc::openvpn::kRemoteCertKU ||
        key == ::onc::openvpn::kServerCAPEMs ||
        key == ::onc::openvpn::kExtraHosts) {
      translated = it.second.Clone();
    } else {
      // Shill wants all Provider/VPN fields to be strings.
      translated = ConvertVpnValueToString(it.second);
    }
    if (!translated.is_none())
      AddValueAccordingToSignature(key, translated);
  }
}

void LocalTranslator::TranslateIPsec() {
  const auto ike_version = onc_object_->FindInt(::onc::ipsec::kIKEVersion);
  if (ike_version.has_value() && ike_version.value() == 2) {
    SetClientCertProperties(client_cert::ConfigType::kIkev2, *onc_object_,
                            shill_dictionary_);

    // The translation table set in this object is for L2TP/IPsec, so we do the
    // copy manually.
    CopyFieldFromONCToShill(::onc::ipsec::kAuthenticationType,
                            shill::kIKEv2AuthenticationTypeProperty);
    CopyFieldFromONCToShill(::onc::ipsec::kPSK, shill::kIKEv2PskProperty);
    CopyFieldFromONCToShill(::onc::ipsec::kServerCAPEMs,
                            shill::kIKEv2CaCertPemProperty);
    CopyFieldFromONCToShill(::onc::ipsec::kLocalIdentity,
                            shill::kIKEv2LocalIdentityProperty);
    CopyFieldFromONCToShill(::onc::ipsec::kRemoteIdentity,
                            shill::kIKEv2RemoteIdentityProperty);
  } else {
    // For L2TP/IPsec.
    SetClientCertProperties(client_cert::ConfigType::kL2tpIpsec, *onc_object_,
                            shill_dictionary_);
    CopyFieldsAccordingToSignature();
  }

  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  CopyFieldFromONCToShill(::onc::vpn::kSaveCredentials,
                          shill::kSaveCredentialsProperty);
}

void LocalTranslator::TranslateL2TP() {
  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  CopyFieldFromONCToShill(::onc::vpn::kSaveCredentials,
                          shill::kSaveCredentialsProperty);

  const base::Value* lcp_echo_disabled =
      onc_object_->Find(::onc::l2tp::kLcpEchoDisabled);
  if (lcp_echo_disabled) {
    base::Value lcp_echo_disabled_value =
        ConvertVpnValueToString(*lcp_echo_disabled);
    shill_dictionary_->Set(shill::kL2TPIPsecLcpEchoDisabledProperty,
                           std::move(lcp_echo_disabled_value));
  }

  // Set shill::kL2TPIPsecUseLoginPasswordProperty according to whether or not
  // the password substitution variable is set.
  const std::string* password = onc_object_->FindString(::onc::l2tp::kPassword);
  if (password &&
      *password == ::onc::substitutes::kPasswordPlaceholderVerbatim) {
    // TODO(b/220249018): shill::kL2tpIpsecUseLoginPasswordProperty is a string
    // property containing "false" or "true". Migrate it to a bool to match
    // shill::kEapUseLoginPasswordProperty.
    shill_dictionary_->Set(shill::kL2TPIPsecUseLoginPasswordProperty, "true");
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateVPN() {
  const std::string* onc_type = onc_object_->FindString(::onc::vpn::kType);
  if (onc_type) {
    TranslateWithTableAndSet(*onc_type, kVPNTypeTable,
                             shill::kProviderTypeProperty);
  }
  if (onc_type && *onc_type == ::onc::vpn::kThirdPartyVpn) {
    // For third-party VPNs, |shill::kProviderHostProperty| is used to store the
    // provider's extension ID.
    const base::Value::Dict* onc_third_party_vpn =
        onc_object_->FindDict(::onc::vpn::kThirdPartyVpn);
    if (onc_third_party_vpn) {
      const std::string* onc_extension_id =
          onc_third_party_vpn->FindString(::onc::third_party_vpn::kExtensionID);
      if (onc_extension_id) {
        shill_dictionary_->Set(shill::kProviderHostProperty, *onc_extension_id);
      }
    }
  } else {
    CopyFieldFromONCToShill(::onc::vpn::kHost, shill::kProviderHostProperty);
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateWiFi() {
  const std::string* security = onc_object_->FindString(::onc::wifi::kSecurity);
  if (security) {
    TranslateWithTableAndSet(*security, kWiFiSecurityTable,
                             shill::kSecurityClassProperty);
    if (*security == ::onc::wifi::kWEP_8021X) {
      shill_dictionary_->Set(shill::kEapKeyMgmtProperty,
                             shill::kKeyManagementIEEE8021X);
    }
  }

  // We currently only support managed and no adhoc networks.
  shill_dictionary_->Set(shill::kModeProperty, shill::kModeManaged);

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateEAP() {
  const std::string outer =
      FindStringKeyOrEmpty(*onc_object_, ::onc::eap::kOuter);
  if (!outer.empty())
    TranslateWithTableAndSet(outer, kEAPOuterTable, shill::kEapMethodProperty);

  // Translate the inner protocol only for outer tunneling protocols.
  if (outer == ::onc::eap::kPEAP || outer == ::onc::eap::kEAP_TTLS) {
    std::string inner = FindStringKeyOrEmpty(*onc_object_, ::onc::eap::kInner);
    // In ONC the Inner protocol defaults to "Automatic".
    if (inner.empty())
      inner = ::onc::eap::kAutomatic;
    // ONC's Inner == "Automatic" translates to omitting the Phase2 property in
    // Shill.
    if (inner != ::onc::eap::kAutomatic) {
      const StringTranslationEntry* table =
          GetEapInnerTranslationTableForOncOuter(outer);
      if (table) {
        TranslateWithTableAndSet(inner, table, shill::kEapPhase2AuthProperty);
      }
    }
  }

  SetClientCertProperties(client_cert::ConfigType::kEap, *onc_object_,
                          shill_dictionary_);

  // Set shill::kEapUseLoginPasswordProperty according to whether or not the
  // password substitution variable is set.
  const std::string* password_field =
      onc_object_->FindString(::onc::eap::kPassword);
  if (password_field &&
      *password_field == ::onc::substitutes::kPasswordPlaceholderVerbatim) {
    shill_dictionary_->Set(shill::kEapUseLoginPasswordProperty, true);
  }

  // Set shill::kEapSubjectAlternativeNameMatchProperty to the serialized form
  // of the subject alternative name match list of dictionaries.
  const base::Value::List* subject_alternative_name_match =
      onc_object_->FindList(::onc::eap::kSubjectAlternativeNameMatch);
  if (subject_alternative_name_match) {
    base::Value::List serialized_dicts;
    std::string serialized_dict;
    JSONStringValueSerializer serializer(&serialized_dict);
    for (const base::Value& v : *subject_alternative_name_match) {
      if (serializer.Serialize(v)) {
        serialized_dicts.Append(serialized_dict);
      }
    }
    shill_dictionary_->Set(shill::kEapSubjectAlternativeNameMatchProperty,
                           std::move(serialized_dicts));
  }

  CopyFieldsAccordingToSignature();

  // Set value or an empty list for ServerCAPEMs if it is not provided by onc.
  // It will override the previous known list during properties merge.
  if (onc_object_->contains(::onc::eap::kServerCAPEMs)) {
    CopyFieldFromONCToShill(::onc::eap::kServerCAPEMs,
                            shill::kEapCaCertPemProperty);
  } else {
    bool is_supported_ca_pem_protocols =
        (outer == ::onc::eap::kEAP_TLS || outer == ::onc::eap::kEAP_TTLS ||
         outer == ::onc::eap::kPEAP);
    if (is_supported_ca_pem_protocols) {
      shill_dictionary_->Set(shill::kEapCaCertPemProperty, base::Value::List());
    }
  }
}

void LocalTranslator::TranslateStaticIPConfig() {
  CopyFieldsAccordingToSignature();
  // Shill expects 4 valid nameserver values. Ensure all values are valid and
  // replace any invalid values with 0.0.0.0 (which has no effect). See
  // https://crbug.com/922219 for details.
  base::Value::List* name_servers =
      shill_dictionary_->FindList(shill::kNameServersProperty);
  if (name_servers) {
    static const char kDefaultIpAddr[] = "0.0.0.0";
    net::IPAddress ip_addr;
    for (base::Value& value_ref : *name_servers) {
      // AssignFromIPLiteral returns true if a string is valid ipv4 or ipv6.
      if (!ip_addr.AssignFromIPLiteral(value_ref.GetString()))
        value_ref = base::Value(kDefaultIpAddr);
    }
    while (name_servers->size() < 4) {
      name_servers->Append(base::Value(kDefaultIpAddr));
    }
  }
}

void LocalTranslator::TranslateNetworkConfiguration() {
  const std::string type =
      FindStringKeyOrEmpty(*onc_object_, ::onc::network_config::kType);
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

  const std::string ip_address_config_type = FindStringKeyOrEmpty(
      *onc_object_, ::onc::network_config::kIPAddressConfigType);
  const std::string name_servers_config_type = FindStringKeyOrEmpty(
      *onc_object_, ::onc::network_config::kNameServersConfigType);
  if ((ip_address_config_type == ::onc::network_config::kIPConfigTypeDHCP) &&
      (name_servers_config_type == ::onc::network_config::kIPConfigTypeDHCP)) {
    // If neither type is set to Static, provide an empty dictionary to ensure
    // that any unset properties are cleared.
    // Note: A type defaults to DHCP if not specified.
    // TODO(b/245885527): Come up with a better way to handle ONC defaults.
    shill_dictionary_->Set(shill::kStaticIPConfigProperty, base::Value::Dict());
  }

  const base::Value::Dict* proxy_settings =
      onc_object_->FindDict(::onc::network_config::kProxySettings);
  if (proxy_settings) {
    base::Value::Dict proxy_config =
        ConvertOncProxySettingsToProxyConfig(*proxy_settings)
            .value_or(base::Value::Dict());
    std::string proxy_config_str;
    base::JSONWriter::Write(proxy_config, &proxy_config_str);
    shill_dictionary_->Set(shill::kProxyConfigProperty, proxy_config_str);
  }

  const std::string* checkCaptivePortal =
      onc_object_->FindString(::onc::network_config::kCheckCaptivePortal);
  if (checkCaptivePortal) {
    TranslateWithTableAndSet(*checkCaptivePortal,
                             kCheckCaptivePortalTranslationTable,
                             shill::kCheckPortalProperty);
  }
  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateCellular() {
  // User APNs for a Cellular network can be enabled/disabled by the user.
  // Shill should only get enabled user APNs to create the data connection.
  if (const base::Value::List* user_apn_list =
          onc_object_->FindList(::onc::cellular::kCustomAPNList)) {
    base::Value::List enabled_apns;
    for (const base::Value& apn : *user_apn_list) {
      const std::string& state =
          FindStringKeyOrEmpty(apn.GetDict(), ::onc::cellular_apn::kState);
      if (state != ::onc::cellular_apn::kStateEnabled)
        continue;

      base::Value::Dict shill_apn;
      LocalTranslator translator(chromeos::onc::kCellularApnSignature,
                                 apn.GetDict(), &shill_apn);
      translator.TranslateFields();
      enabled_apns.Append(std::move(shill_apn));
    }
    shill_dictionary_->Set(shill::kCellularCustomApnListProperty,
                           base::Value(std::move(enabled_apns)));
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateApn() {
  const std::string* authentication =
      onc_object_->FindString(::onc::cellular_apn::kAuthentication);
  if (authentication) {
    TranslateWithTableAndSet(*authentication,
                             kApnAuthenticationTranslationTable,
                             shill::kApnAuthenticationProperty);
  }

  if (!ash::features::IsApnRevampEnabled()) {
    CopyFieldsAccordingToSignature();
    return;
  }

  const std::string* ip_type =
      onc_object_->FindString(::onc::cellular_apn::kIpType);
  if (ip_type) {
    TranslateWithTableAndSet(*ip_type, kApnIpTypeTranslationTable,
                             shill::kApnIpTypeProperty);
  }

  const base::Value::List* apn_types =
      onc_object_->FindList(::onc::cellular_apn::kApnTypes);
  DCHECK(apn_types) << "APN must have APN types";

  if (ash::features::IsApnRevampAndPoliciesEnabled()) {
    const std::string* apn_source =
        onc_object_->FindString(::onc::cellular_apn::kSource);
    if (apn_source) {
      // APNs being translated from ONC to Shill should only ever be provided by
      // an admin or by the user via the UI.
      const bool is_unexpected_source =
          *apn_source != ::onc::cellular_apn::kSourceAdmin &&
          *apn_source != ::onc::cellular_apn::kSourceUi;

      if (is_unexpected_source) {
        NET_LOG(ERROR) << R"(Unexpected ONC to Shill APN source type of ")"
                       << *apn_source
                       << R"(". Setting Shill APN source type to ")"
                       << shill::kApnSourceUi << R"(".)";

        shill_dictionary_->Set(shill::kApnSourceProperty, shill::kApnSourceUi);
      } else {
        TranslateWithTableAndSet(*apn_source, kApnSourceTranslationTable,
                                 shill::kApnSourceProperty);
      }
    } else {
      // Shill expects that APNs provided Chrome will only ever be provided by
      // the UI or by an admin. We default to the source being the UI but check
      // if it was provided by an admin. For more information see
      // b/329714110#comment5 and b/333100319.
      shill_dictionary_->Set(shill::kApnSourceProperty, shill::kApnSourceUi);
    }
  } else {
    shill_dictionary_->Set(shill::kApnSourceProperty, shill::kApnSourceUi);
  }

  // Convert array of APN types to comma-delimited, de-duped string, i.e.
  // ["Default", "Attach", "Default"] -> "DEFAULT,IA".
  bool contains_default = false;
  bool contains_attach = false;
  bool contains_tether = false;
  for (const auto& apn_type : *apn_types) {
    std::string apn_type_string = apn_type.GetString();
    if (apn_type_string == ::onc::cellular_apn::kApnTypeDefault) {
      contains_default = true;
    } else if (apn_type_string == ::onc::cellular_apn::kApnTypeAttach) {
      contains_attach = true;
    } else if (apn_type_string == ::onc::cellular_apn::kApnTypeTether) {
      contains_tether = true;
    } else {
      NOTREACHED_IN_MIGRATION() << "Invalid APN type: " << apn_type;
    }
  }
  std::vector<std::string> apn_type_strings;
  if (contains_default) {
    apn_type_strings.push_back(shill::kApnTypeDefault);
  }
  if (contains_attach) {
    apn_type_strings.push_back(shill::kApnTypeIA);
  }
  if (contains_tether) {
    apn_type_strings.push_back(shill::kApnTypeDun);
  }

  const std::string apn_types_string = base::JoinString(apn_type_strings, ",");
  if (apn_types_string.empty()) {
    NET_LOG(ERROR) << "APN must have at least one APN type";
  }
  shill_dictionary_->Set(shill::kApnTypesProperty, apn_types_string);

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::CopyFieldsAccordingToSignature() {
  for (const auto it : *onc_object_) {
    AddValueAccordingToSignature(it.first, it.second);
  }
}

void LocalTranslator::CopyFieldFromONCToShill(
    const std::string& onc_field_name,
    const std::string& shill_property_name) {
  const base::Value* value = onc_object_->Find(onc_field_name);
  if (!value)
    return;

  const chromeos::onc::OncFieldSignature* field_signature =
      chromeos::onc::GetFieldSignature(*onc_signature_, onc_field_name);
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
  shill_dictionary_->Set(shill_property_name, value->Clone());
}

void LocalTranslator::AddValueAccordingToSignature(const std::string& onc_name,
                                                   const base::Value& value) {
  if (value.is_none() || !field_translation_table_)
    return;
  std::string shill_property_name;
  if (!GetShillPropertyName(onc_name, field_translation_table_,
                            &shill_property_name)) {
    return;
  }
  shill_dictionary_->Set(shill_property_name, value.Clone());
}

void LocalTranslator::TranslateWithTableAndSet(
    const std::string& onc_value,
    const StringTranslationEntry table[],
    const std::string& shill_property_name) {
  std::string shill_value;
  if (TranslateStringToShill(table, onc_value, &shill_value)) {
    shill_dictionary_->Set(shill_property_name, shill_value);
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
void TranslateONCHierarchy(const chromeos::onc::OncValueSignature& signature,
                           const base::Value::Dict& onc_object,
                           base::Value::Dict& shill_dictionary) {
  const std::vector<std::string> path =
      GetPathToNestedShillDictionary(signature);
  base::Value::Dict* target_shill_dictionary = &shill_dictionary;
  for (const std::string& path_piece : path) {
    target_shill_dictionary = target_shill_dictionary->EnsureDict(path_piece);
  }

  // Translates fields of |onc_object| and writes them to
  // |target_shill_dictionary_| nested in |shill_dictionary|.
  LocalTranslator translator(signature, onc_object, target_shill_dictionary);
  translator.TranslateFields();

  // Recurse into nested objects.
  for (const auto it : onc_object) {
    if (!it.second.is_dict())
      continue;

    const chromeos::onc::OncFieldSignature* field_signature =
        chromeos::onc::GetFieldSignature(signature, it.first);
    if (!field_signature) {
      NET_LOG(ERROR) << "Unexpected or deprecated ONC key: " << it.first;
      continue;
    }
    TranslateONCHierarchy(*field_signature->value_signature,
                          it.second.GetDict(), shill_dictionary);
  }
}

}  // namespace

base::Value::Dict TranslateONCObjectToShill(
    const chromeos::onc::OncValueSignature* onc_signature,
    const base::Value::Dict& onc_object) {
  CHECK(onc_signature != nullptr);
  base::Value::Dict shill_dictionary;
  TranslateONCHierarchy(*onc_signature, onc_object, shill_dictionary);
  return shill_dictionary;
}

}  // namespace ash::onc
