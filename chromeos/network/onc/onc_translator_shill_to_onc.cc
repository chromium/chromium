// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

namespace {

// Converts |str| to a base::Value of the given |type|. If the conversion fails,
// returns NULL.
std::unique_ptr<base::Value> ConvertStringToValue(const std::string& str,
                                                  base::Value::Type type) {
  std::unique_ptr<base::Value> value;
  if (type == base::Value::Type::STRING) {
    value.reset(new base::Value(str));
  } else {
    value = base::JSONReader::ReadDeprecated(str);
  }
  if (value && value->type() != type)
    return nullptr;

  return value;
}

// If the network is configured with an installed certificate, a PKCS11 id will
// be set which is provided for the UI to display certificate information.
// Returns true if the PKCS11 id is available and set.
bool SetPKCS11Id(const base::DictionaryValue* shill_dictionary,
                 const char* cert_id_property,
                 const char* cert_slot_property,
                 base::DictionaryValue* onc_object) {
  std::string shill_cert_id;
  if (!shill_dictionary->GetStringWithoutPathExpansion(cert_id_property,
                                                       &shill_cert_id) ||
      shill_cert_id.empty()) {
    return false;
  }

  std::string shill_slot;
  std::string pkcs11_id;
  if (shill_dictionary->GetStringWithoutPathExpansion(cert_slot_property,
                                                      &shill_slot) &&
      !shill_slot.empty()) {
    pkcs11_id = shill_slot + ":" + shill_cert_id;
  } else {
    pkcs11_id = shill_cert_id;
  }

  onc_object->SetKey(::onc::client_cert::kClientCertType,
                     base::Value(::onc::client_cert::kPKCS11Id));
  onc_object->SetKey(::onc::client_cert::kClientCertPKCS11Id,
                     base::Value(pkcs11_id));
  return true;
}

// This class implements the translation of properties from the given
// |shill_dictionary| to a new ONC object of signature |onc_signature|. Using
// recursive calls to CreateTranslatedONCObject of new instances, nested objects
// are translated.
class ShillToONCTranslator {
 public:
  ShillToONCTranslator(const base::DictionaryValue& shill_dictionary,
                       ::onc::ONCSource onc_source,
                       const OncValueSignature& onc_signature,
                       const NetworkState* network_state)
      : shill_dictionary_(&shill_dictionary),
        onc_source_(onc_source),
        onc_signature_(&onc_signature),
        network_state_(network_state) {
    field_translation_table_ = GetFieldTranslationTable(onc_signature);
  }

  ShillToONCTranslator(const base::DictionaryValue& shill_dictionary,
                       ::onc::ONCSource onc_source,
                       const OncValueSignature& onc_signature,
                       const FieldTranslationEntry* field_translation_table,
                       const NetworkState* network_state)
      : shill_dictionary_(&shill_dictionary),
        onc_source_(onc_source),
        onc_signature_(&onc_signature),
        field_translation_table_(field_translation_table),
        network_state_(network_state) {}

  // Translates the associated Shill dictionary and creates an ONC object of the
  // given signature.
  std::unique_ptr<base::DictionaryValue> CreateTranslatedONCObject();

 private:
  void TranslateEthernet();
  void TranslateOpenVPN();
  void TranslateIPsec();
  void TranslateThirdPartyVPN();
  void TranslateVPN();
  void TranslateWiFiWithState();
  void TranslateCellularWithState();
  void TranslateCellularDevice();
  void TranslateNetworkWithState();
  void TranslateIPConfig();
  void TranslateSavedOrStaticIPConfig();
  void TranslateSavedIPConfig();
  void TranslateStaticIPConfig();
  void TranslateEap();

  // Creates an ONC object from |dictionary| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name,
                                   const base::DictionaryValue& dictionary);

  // Creates an ONC object from |shill_dictionary_| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name);

  // Sets |onc_field_name| in dictionary |onc_dictionary_name| in |onc_object_|
  // to |value| if the dictionary exists.
  void SetNestedOncValue(const std::string& onc_dictionary_name,
                         const std::string& onc_field_name,
                         const base::Value& value);

  // Translates a list of nested objects and adds the list to |onc_object_| at
  // |onc_field_name|. If there are errors while parsing individual objects or
  // if the resulting list contains no entries, the result will not be added to
  // |onc_object_|.
  void TranslateAndAddListOfObjects(const std::string& onc_field_name,
                                    const base::ListValue& list);

  // Applies function CopyProperty to each field of |value_signature| and its
  // base signatures.
  void CopyPropertiesAccordingToSignature(
      const OncValueSignature* value_signature);

  // Applies function CopyProperty to each field of |onc_signature_| and its
  // base signatures.
  void CopyPropertiesAccordingToSignature();

  // If |shill_property_name| is defined in |field_signature|, copies this
  // entry from |shill_dictionary_| to |onc_object_| if it exists.
  void CopyProperty(const OncFieldSignature* field_signature);

  // If existent, translates the entry at |shill_property_name| in
  // |shill_dictionary_| using |table|. It is an error if no matching table
  // entry is found. Writes the result as entry at |onc_field_name| in
  // |onc_object_|.
  void TranslateWithTableAndSet(const std::string& shill_property_name,
                                const StringTranslationEntry table[],
                                const std::string& onc_field_name);

  // Returns the name of the Shill service provided in |shill_dictionary_|
  // for debugging.
  std::string GetName();

  const base::DictionaryValue* shill_dictionary_;
  ::onc::ONCSource onc_source_;
  const OncValueSignature* onc_signature_;
  const FieldTranslationEntry* field_translation_table_;
  std::unique_ptr<base::DictionaryValue> onc_object_;
  const NetworkState* network_state_;

  DISALLOW_COPY_AND_ASSIGN(ShillToONCTranslator);
};

std::unique_ptr<base::DictionaryValue>
ShillToONCTranslator::CreateTranslatedONCObject() {
  onc_object_.reset(new base::DictionaryValue);
  if (onc_signature_ == &kNetworkWithStateSignature) {
    TranslateNetworkWithState();
  } else if (onc_signature_ == &kEthernetSignature) {
    TranslateEthernet();
  } else if (onc_signature_ == &kVPNSignature) {
    TranslateVPN();
  } else if (onc_signature_ == &kOpenVPNSignature) {
    TranslateOpenVPN();
  } else if (onc_signature_ == &kIPsecSignature) {
    TranslateIPsec();
  } else if (onc_signature_ == &kThirdPartyVPNSignature) {
    TranslateThirdPartyVPN();
  } else if (onc_signature_ == &kWiFiWithStateSignature) {
    TranslateWiFiWithState();
  } else if (onc_signature_ == &kCellularWithStateSignature) {
    if (field_translation_table_ == kCellularDeviceTable)
      TranslateCellularDevice();
    else
      TranslateCellularWithState();
  } else if (onc_signature_ == &kIPConfigSignature) {
    TranslateIPConfig();
  } else if (onc_signature_ == &kSavedIPConfigSignature) {
    TranslateSavedIPConfig();
  } else if (onc_signature_ == &kStaticIPConfigSignature) {
    TranslateStaticIPConfig();
  } else if (onc_signature_ == &kEAPSignature) {
    TranslateEap();
  } else {
    CopyPropertiesAccordingToSignature();
  }
  return std::move(onc_object_);
}

void ShillToONCTranslator::TranslateEthernet() {
  std::string shill_network_type;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                   &shill_network_type);
  const char* onc_auth = ::onc::ethernet::kAuthenticationNone;
  if (shill_network_type == shill::kTypeEthernetEap)
    onc_auth = ::onc::ethernet::k8021X;
  onc_object_->SetKey(::onc::ethernet::kAuthentication, base::Value(onc_auth));

  if (shill_network_type == shill::kTypeEthernetEap)
    TranslateAndAddNestedObject(::onc::ethernet::kEAP);
}

void ShillToONCTranslator::TranslateOpenVPN() {
  if (shill_dictionary_->HasKey(shill::kOpenVPNVerifyX509NameProperty))
    TranslateAndAddNestedObject(::onc::openvpn::kVerifyX509);

  // Shill supports only one RemoteCertKU but ONC requires a list. If existing,
  // wraps the value into a list.
  std::string certKU;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kOpenVPNRemoteCertKUProperty, &certKU)) {
    std::unique_ptr<base::ListValue> certKUs(new base::ListValue);
    certKUs->AppendString(certKU);
    onc_object_->SetWithoutPathExpansion(::onc::openvpn::kRemoteCertKU,
                                         std::move(certKUs));
  }

  SetPKCS11Id(shill_dictionary_, shill::kOpenVPNClientCertIdProperty,
              shill::kOpenVPNClientCertSlotProperty, onc_object_.get());

  for (const OncFieldSignature* field_signature = onc_signature_->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    const std::string& onc_field_name = field_signature->onc_field_name;
    if (onc_field_name == ::onc::openvpn::kRemoteCertKU ||
        onc_field_name == ::onc::openvpn::kServerCAPEMs) {
      CopyProperty(field_signature);
      continue;
    }

    std::string shill_property_name;
    const base::Value* shill_value = NULL;
    if (!field_translation_table_ ||
        !GetShillPropertyName(field_signature->onc_field_name,
                              field_translation_table_, &shill_property_name) ||
        !shill_dictionary_->GetWithoutPathExpansion(shill_property_name,
                                                    &shill_value)) {
      continue;
    }

    std::unique_ptr<base::Value> translated;
    std::string shill_str;
    if (shill_value->GetAsString(&shill_str)) {
      // Shill wants all Provider/VPN fields to be strings. Translates these
      // strings back to the correct ONC type.
      translated = ConvertStringToValue(
          shill_str, field_signature->value_signature->onc_type);

      if (translated.get() == NULL) {
        LOG(ERROR) << "Shill property '" << shill_property_name
                   << "' with value " << *shill_value
                   << " couldn't be converted to base::Value::Type "
                   << field_signature->value_signature->onc_type << ": "
                   << GetName();
      } else {
        onc_object_->SetWithoutPathExpansion(onc_field_name,
                                             std::move(translated));
      }
    } else {
      LOG(ERROR) << "Shill property '" << shill_property_name << "' has value "
                 << *shill_value << ", but expected a string: " << GetName();
    }
  }
}

void ShillToONCTranslator::TranslateIPsec() {
  CopyPropertiesAccordingToSignature();
  if (shill_dictionary_->HasKey(shill::kL2tpIpsecXauthUserProperty))
    TranslateAndAddNestedObject(::onc::ipsec::kXAUTH);

  std::string authentication_type;
  if (SetPKCS11Id(shill_dictionary_, shill::kL2tpIpsecClientCertIdProperty,
                  shill::kL2tpIpsecClientCertSlotProperty, onc_object_.get())) {
    authentication_type = ::onc::ipsec::kCert;
  } else {
    authentication_type = ::onc::ipsec::kPSK;
  }
  onc_object_->SetKey(::onc::ipsec::kAuthenticationType,
                      base::Value(authentication_type));
}

void ShillToONCTranslator::TranslateThirdPartyVPN() {
  CopyPropertiesAccordingToSignature();

  // For third-party VPNs, |shill::kProviderHostProperty| is used to store the
  // provider's extension ID.
  std::string shill_extension_id;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kHostProperty,
                                                   &shill_extension_id);
  onc_object_->SetKey(::onc::third_party_vpn::kExtensionID,
                      base::Value(shill_extension_id));
}

void ShillToONCTranslator::TranslateVPN() {
  CopyPropertiesAccordingToSignature();

  // Parse Shill Provider dictionary. Note, this may not exist, e.g. if we are
  // just translating network state in network_util::TranslateNetworkStateToONC.
  const base::DictionaryValue* provider = NULL;
  if (!shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kProviderProperty, &provider)) {
    return;
  }
  std::string shill_provider_type, onc_provider_type;
  provider->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                          &shill_provider_type);
  if (!TranslateStringToONC(kVPNTypeTable, shill_provider_type,
                            &onc_provider_type)) {
    return;
  }
  onc_object_->SetKey(::onc::vpn::kType, base::Value(onc_provider_type));
  std::string shill_provider_host;
  if (onc_provider_type != ::onc::vpn::kThirdPartyVpn &&
      provider->GetStringWithoutPathExpansion(shill::kHostProperty,
                                              &shill_provider_host)) {
    onc_object_->SetKey(::onc::vpn::kHost, base::Value(shill_provider_host));
  }

  // Translate the nested dictionary.
  std::string provider_type_dictionary;
  if (onc_provider_type == ::onc::vpn::kTypeL2TP_IPsec) {
    TranslateAndAddNestedObject(::onc::vpn::kIPsec, *provider);
    TranslateAndAddNestedObject(::onc::vpn::kL2TP, *provider);
    provider_type_dictionary = ::onc::vpn::kIPsec;
  } else {
    TranslateAndAddNestedObject(onc_provider_type, *provider);
    provider_type_dictionary = onc_provider_type;
  }

  bool save_credentials;
  if (onc_provider_type != ::onc::vpn::kThirdPartyVpn &&
      onc_provider_type != ::onc::vpn::kArcVpn &&
      shill_dictionary_->GetBooleanWithoutPathExpansion(
          shill::kSaveCredentialsProperty, &save_credentials)) {
    SetNestedOncValue(provider_type_dictionary, ::onc::vpn::kSaveCredentials,
                      base::Value(save_credentials));
  }
}

void ShillToONCTranslator::TranslateWiFiWithState() {
  std::string shill_security;
  std::string shill_key_mgmt;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kSecurityClassProperty, &shill_security) &&
      shill_security == shill::kSecurityWep &&
      shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kEapKeyMgmtProperty, &shill_key_mgmt) &&
      shill_key_mgmt == shill::kKeyManagementIEEE8021X) {
    onc_object_->SetKey(::onc::wifi::kSecurity,
                        base::Value(::onc::wifi::kWEP_8021X));
  } else {
    TranslateWithTableAndSet(shill::kSecurityClassProperty, kWiFiSecurityTable,
                             ::onc::wifi::kSecurity);
  }

  bool unknown_encoding = true;
  std::string ssid = shill_property_util::GetSSIDFromProperties(
      *shill_dictionary_, false /* verbose_logging */, &unknown_encoding);
  if (!unknown_encoding && !ssid.empty())
    onc_object_->SetKey(::onc::wifi::kSSID, base::Value(ssid));

  bool link_monitor_disable;
  if (shill_dictionary_->GetBooleanWithoutPathExpansion(
          shill::kLinkMonitorDisableProperty, &link_monitor_disable)) {
    onc_object_->SetKey(::onc::wifi::kAllowGatewayARPPolling,
                        base::Value(!link_monitor_disable));
  }

  CopyPropertiesAccordingToSignature();
  TranslateAndAddNestedObject(::onc::wifi::kEAP);
}

void ShillToONCTranslator::TranslateCellularWithState() {
  CopyPropertiesAccordingToSignature();
  TranslateWithTableAndSet(shill::kActivationStateProperty,
                           kActivationStateTable,
                           ::onc::cellular::kActivationState);
  TranslateWithTableAndSet(shill::kNetworkTechnologyProperty,
                           kNetworkTechnologyTable,
                           ::onc::cellular::kNetworkTechnology);
  const base::DictionaryValue* dictionary = NULL;
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kServingOperatorProperty, &dictionary)) {
    TranslateAndAddNestedObject(::onc::cellular::kServingOperator, *dictionary);
  }
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kCellularApnProperty, &dictionary)) {
    TranslateAndAddNestedObject(::onc::cellular::kAPN, *dictionary);
  }
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kCellularLastGoodApnProperty, &dictionary)) {
    TranslateAndAddNestedObject(::onc::cellular::kLastGoodAPN, *dictionary);
  }
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kPaymentPortalProperty, &dictionary)) {
    TranslateAndAddNestedObject(::onc::cellular::kPaymentPortal, *dictionary);
  }

  const base::DictionaryValue* device_dictionary = NULL;
  bool requires_roaming = false;
  bool scanning = false;
  shill_dictionary_->GetDictionaryWithoutPathExpansion(shill::kDeviceProperty,
                                                       &device_dictionary);
  if (device_dictionary) {
    // Merge the Device dictionary with this one (Cellular) using the
    // CellularDevice signature.
    ShillToONCTranslator nested_translator(
        *device_dictionary, onc_source_, kCellularWithStateSignature,
        kCellularDeviceTable, network_state_);
    std::unique_ptr<base::DictionaryValue> nested_object =
        nested_translator.CreateTranslatedONCObject();
    onc_object_->MergeDictionary(nested_object.get());

    /// Get requires_roaming and scanning from the Device dictionary.
    device_dictionary->GetBooleanWithoutPathExpansion(
        shill::kProviderRequiresRoamingProperty, &requires_roaming);
    device_dictionary->GetBooleanWithoutPathExpansion(shill::kScanningProperty,
                                                      &scanning);
  }
  if (requires_roaming) {
    onc_object_->SetKey(::onc::cellular::kRoamingState,
                        base::Value(::onc::cellular::kRoamingRequired));
  } else {
    TranslateWithTableAndSet(shill::kRoamingStateProperty, kRoamingStateTable,
                             ::onc::cellular::kRoamingState);
  }
  onc_object_->SetKey(::onc::cellular::kScanning, base::Value(scanning));
}

void ShillToONCTranslator::TranslateCellularDevice() {
  CopyPropertiesAccordingToSignature();
  const base::DictionaryValue* shill_sim_lock_status = NULL;
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kSIMLockStatusProperty, &shill_sim_lock_status)) {
    TranslateAndAddNestedObject(::onc::cellular::kSIMLockStatus,
                                *shill_sim_lock_status);
  }
  const base::DictionaryValue* shill_home_provider = NULL;
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kHomeProviderProperty, &shill_home_provider)) {
    TranslateAndAddNestedObject(::onc::cellular::kHomeProvider,
                                *shill_home_provider);
  }
  const base::ListValue* shill_apns = NULL;
  if (shill_dictionary_->GetListWithoutPathExpansion(
          shill::kCellularApnListProperty, &shill_apns)) {
    TranslateAndAddListOfObjects(::onc::cellular::kAPNList, *shill_apns);
  }
  const base::ListValue* shill_found_networks = NULL;
  if (shill_dictionary_->GetListWithoutPathExpansion(
          shill::kFoundNetworksProperty, &shill_found_networks)) {
    TranslateAndAddListOfObjects(::onc::cellular::kFoundNetworks,
                                 *shill_found_networks);
  }
}

void ShillToONCTranslator::TranslateNetworkWithState() {
  CopyPropertiesAccordingToSignature();

  std::string shill_network_type;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                   &shill_network_type);
  std::string onc_network_type = ::onc::network_type::kEthernet;
  if (shill_network_type != shill::kTypeEthernet &&
      shill_network_type != shill::kTypeEthernetEap) {
    TranslateStringToONC(kNetworkTypeTable, shill_network_type,
                         &onc_network_type);
  }
  // Translate nested Cellular, WiFi, etc. properties.
  if (!onc_network_type.empty()) {
    onc_object_->SetKey(::onc::network_config::kType,
                        base::Value(onc_network_type));
    TranslateAndAddNestedObject(onc_network_type);
  }

  // Since Name is a read only field in Shill unless it's a VPN, it is copied
  // here, but not when going the other direction (if it's not a VPN).
  std::string name;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kNameProperty, &name);
  onc_object_->SetKey(::onc::network_config::kName, base::Value(name));

  // Limit ONC state to "NotConnected", "Connected", or "Connecting".
  std::string state;
  if (shill_dictionary_->GetStringWithoutPathExpansion(shill::kStateProperty,
                                                       &state)) {
    std::string onc_state = ::onc::connection_state::kNotConnected;
    if (NetworkState::StateIsConnected(state)) {
      onc_state = ::onc::connection_state::kConnected;
    } else if (NetworkState::StateIsConnecting(state)) {
      onc_state = ::onc::connection_state::kConnecting;
    }
    onc_object_->SetKey(::onc::network_config::kConnectionState,
                        base::Value(onc_state));
    // Only set 'RestrictedConnectivity' if captive portal state is true.
    if (NetworkState::NetworkStateIsCaptivePortal(*shill_dictionary_)) {
      onc_object_->SetKey(::onc::network_config::kRestrictedConnectivity,
                          base::Value(true));
    }
  }

  // Non-visible networks (with null network_state_) do not set ErrorState.
  if (network_state_) {
    if (!network_state_->GetError().empty()) {
      onc_object_->SetKey(::onc::network_config::kErrorState,
                          base::Value(network_state_->GetError()));
    }
  }

  std::string profile_path;
  if (onc_source_ != ::onc::ONC_SOURCE_UNKNOWN &&
      shill_dictionary_->GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                       &profile_path)) {
    std::string source;
    if (onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY)
      source = ::onc::network_config::kSourceDevicePolicy;
    else if (onc_source_ == ::onc::ONC_SOURCE_USER_POLICY)
      source = ::onc::network_config::kSourceUserPolicy;
    else if (profile_path == NetworkProfileHandler::GetSharedProfilePath())
      source = ::onc::network_config::kSourceDevice;
    else if (!profile_path.empty())
      source = ::onc::network_config::kSourceUser;
    else
      source = ::onc::network_config::kSourceNone;
    onc_object_->SetKey(::onc::network_config::kSource, base::Value(source));
  }

  // Use a human-readable aa:bb format for any hardware MAC address. Note:
  // this property is provided by the caller but is not part of the Shill
  // Service properties (it is copied from the Device properties).
  std::string address;
  if (shill_dictionary_->GetStringWithoutPathExpansion(shill::kAddressProperty,
                                                       &address)) {
    onc_object_->SetKey(
        ::onc::network_config::kMacAddress,
        base::Value(network_util::FormattedMacAddress(address)));
  }

  // Shill's Service has an IPConfig property (note the singular), not an
  // IPConfigs property. However, we require the caller of the translation to
  // patch the Shill dictionary before passing it to the translator.
  const base::ListValue* shill_ipconfigs = NULL;
  if (shill_dictionary_->GetListWithoutPathExpansion(shill::kIPConfigsProperty,
                                                     &shill_ipconfigs)) {
    TranslateAndAddListOfObjects(::onc::network_config::kIPConfigs,
                                 *shill_ipconfigs);
  }

  const base::DictionaryValue* saved_ipconfig = nullptr;
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kSavedIPConfigProperty, &saved_ipconfig)) {
    TranslateAndAddNestedObject(::onc::network_config::kSavedIPConfig,
                                *saved_ipconfig);
  }

  // Translate the StaticIPConfig object and set the IP config types.
  const base::DictionaryValue* static_ipconfig = nullptr;
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kStaticIPConfigProperty, &static_ipconfig)) {
    std::string ip_address;
    if (static_ipconfig->GetStringWithoutPathExpansion(shill::kAddressProperty,
                                                       &ip_address) &&
        !ip_address.empty()) {
      onc_object_->SetKey(
          ::onc::network_config::kIPAddressConfigType,
          base::Value(::onc::network_config::kIPConfigTypeStatic));
    }
    const base::ListValue* name_servers = nullptr;
    if (static_ipconfig->GetListWithoutPathExpansion(
            shill::kNameServersProperty, &name_servers) &&
        !name_servers->empty()) {
      onc_object_->SetKey(
          ::onc::network_config::kNameServersConfigType,
          base::Value(::onc::network_config::kIPConfigTypeStatic));
    }
    if (!ip_address.empty() || (name_servers && !name_servers->empty())) {
      TranslateAndAddNestedObject(::onc::network_config::kStaticIPConfig,
                                  *static_ipconfig);
    }
  }

  std::string proxy_config_str;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kProxyConfigProperty, &proxy_config_str) &&
      !proxy_config_str.empty()) {
    std::unique_ptr<base::Value> proxy_config_value(
        ReadDictionaryFromJson(proxy_config_str));
    if (proxy_config_value) {
      base::Value proxy_settings =
          ConvertProxyConfigToOncProxySettings(*proxy_config_value);
      if (!proxy_settings.is_none()) {
        onc_object_->SetKey(::onc::network_config::kProxySettings,
                            std::move(proxy_settings));
      }
    }
  }
}

void ShillToONCTranslator::TranslateIPConfig() {
  CopyPropertiesAccordingToSignature();
  std::string shill_ip_method;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kMethodProperty,
                                                   &shill_ip_method);
  std::string type;
  if (shill_ip_method == shill::kTypeIPv4 ||
      shill_ip_method == shill::kTypeDHCP) {
    type = ::onc::ipconfig::kIPv4;
  } else if (shill_ip_method == shill::kTypeIPv6 ||
             shill_ip_method == shill::kTypeDHCP6) {
    type = ::onc::ipconfig::kIPv6;
  } else {
    return;  // Ignore unhandled IPConfig types, e.g. bootp, zeroconf, ppp
  }

  onc_object_->SetKey(::onc::ipconfig::kType, base::Value(type));
}

void ShillToONCTranslator::TranslateSavedOrStaticIPConfig() {
  CopyPropertiesAccordingToSignature();

  // Static and Saved IPConfig in Shill are always of type IPv4. Set this type
  // in ONC, but not if the object would be empty except the type.
  if (!onc_object_->empty()) {
    onc_object_->SetKey(::onc::ipconfig::kType,
                        base::Value(::onc::ipconfig::kIPv4));
  }
}

void ShillToONCTranslator::TranslateSavedIPConfig() {
  TranslateSavedOrStaticIPConfig();
}

void ShillToONCTranslator::TranslateStaticIPConfig() {
  TranslateSavedOrStaticIPConfig();
}

void ShillToONCTranslator::TranslateEap() {
  CopyPropertiesAccordingToSignature();

  // Translate EAP Outer and Inner values if EAP.EAP exists and is not empty.
  std::string shill_eap_method;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kEapMethodProperty, &shill_eap_method) &&
      !shill_eap_method.empty()) {
    TranslateWithTableAndSet(shill::kEapMethodProperty, kEAPOuterTable,
                             ::onc::eap::kOuter);
    std::string shill_phase2_auth;
    if (shill_dictionary_->GetStringWithoutPathExpansion(
            shill::kEapPhase2AuthProperty, &shill_phase2_auth) &&
        !shill_phase2_auth.empty()) {
      TranslateWithTableAndSet(shill::kEapPhase2AuthProperty,
                               kEAP_TTLS_InnerTable, ::onc::eap::kInner);
    }
  }

  std::string shill_cert_id;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kEapCertIdProperty, &shill_cert_id)) {
    onc_object_->SetKey(::onc::client_cert::kClientCertType,
                        base::Value(::onc::client_cert::kPKCS11Id));
    // Note: shill::kEapCertIdProperty is already in the format slot:key_id.
    // Note: shill::kEapKeyIdProperty has the same value as
    //       shill::kEapCertIdProperty and is ignored.
    onc_object_->SetKey(::onc::client_cert::kClientCertPKCS11Id,
                        base::Value(shill_cert_id));
  }

  bool use_login_password = false;
  if (shill_dictionary_->GetBooleanWithoutPathExpansion(
          shill::kEapUseLoginPasswordProperty, &use_login_password) &&
      use_login_password) {
    onc_object_->SetKey(
        ::onc::eap::kPassword,
        base::Value(::onc::substitutes::kPasswordPlaceholderVerbatim));
  }
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name) {
  TranslateAndAddNestedObject(onc_field_name, *shill_dictionary_);
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name,
    const base::DictionaryValue& dictionary) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  if (!field_signature) {
    NOTREACHED() << "Unable to find signature for field: " << onc_field_name;
    return;
  }
  ShillToONCTranslator nested_translator(dictionary, onc_source_,
                                         *field_signature->value_signature,
                                         network_state_);
  std::unique_ptr<base::DictionaryValue> nested_object =
      nested_translator.CreateTranslatedONCObject();
  if (nested_object->empty())
    return;
  onc_object_->SetWithoutPathExpansion(onc_field_name,
                                       std::move(nested_object));
}

void ShillToONCTranslator::SetNestedOncValue(
    const std::string& onc_dictionary_name,
    const std::string& onc_field_name,
    const base::Value& value) {
  onc_object_->SetPath({onc_dictionary_name, onc_field_name}, value.Clone());
}

void ShillToONCTranslator::TranslateAndAddListOfObjects(
    const std::string& onc_field_name,
    const base::ListValue& list) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  if (field_signature->value_signature->onc_type != base::Value::Type::LIST) {
    LOG(ERROR) << "ONC Field name: '" << onc_field_name << "' has type '"
               << field_signature->value_signature->onc_type
               << "', expected: base::Value::Type::LIST: " << GetName();
    return;
  }
  DCHECK(field_signature->value_signature->onc_array_entry_signature);
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (base::ListValue::const_iterator it = list.begin(); it != list.end();
       ++it) {
    const base::DictionaryValue* shill_value = NULL;
    if (!it->GetAsDictionary(&shill_value))
      continue;
    ShillToONCTranslator nested_translator(
        *shill_value, onc_source_,
        *field_signature->value_signature->onc_array_entry_signature,
        network_state_);
    std::unique_ptr<base::DictionaryValue> nested_object =
        nested_translator.CreateTranslatedONCObject();
    // If the nested object couldn't be parsed, simply omit it.
    if (nested_object->empty())
      continue;
    result->Append(std::move(nested_object));
  }
  // If there are no entries in the list, there is no need to expose this field.
  if (result->empty())
    return;
  onc_object_->SetWithoutPathExpansion(onc_field_name, std::move(result));
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature() {
  CopyPropertiesAccordingToSignature(onc_signature_);
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature(
    const OncValueSignature* value_signature) {
  if (value_signature->base_signature)
    CopyPropertiesAccordingToSignature(value_signature->base_signature);
  if (!value_signature->fields)
    return;
  for (const OncFieldSignature* field_signature = value_signature->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    CopyProperty(field_signature);
  }
}

void ShillToONCTranslator::CopyProperty(
    const OncFieldSignature* field_signature) {
  std::string shill_property_name;
  const base::Value* shill_value = NULL;
  if (!field_translation_table_ ||
      !GetShillPropertyName(field_signature->onc_field_name,
                            field_translation_table_, &shill_property_name) ||
      !shill_dictionary_->GetWithoutPathExpansion(shill_property_name,
                                                  &shill_value)) {
    return;
  }

  if (shill_value->type() != field_signature->value_signature->onc_type) {
    LOG(ERROR) << "Shill property '" << shill_property_name << "' with value "
               << *shill_value << " has base::Value::Type "
               << shill_value->type() << " but ONC field '"
               << field_signature->onc_field_name << "' requires type "
               << field_signature->value_signature->onc_type << ": "
               << GetName();
    return;
  }

  onc_object_->SetKey(field_signature->onc_field_name, shill_value->Clone());
}

void ShillToONCTranslator::TranslateWithTableAndSet(
    const std::string& shill_property_name,
    const StringTranslationEntry table[],
    const std::string& onc_field_name) {
  std::string shill_value;
  if (!shill_dictionary_->GetStringWithoutPathExpansion(shill_property_name,
                                                        &shill_value) ||
      shill_value.empty()) {
    return;
  }
  std::string onc_value;
  if (TranslateStringToONC(table, shill_value, &onc_value)) {
    onc_object_->SetKey(onc_field_name, base::Value(onc_value));
    return;
  }
  LOG(ERROR) << "Shill property '" << shill_property_name << "' with value "
             << shill_value << " couldn't be translated to ONC: " << GetName();
}

std::string ShillToONCTranslator::GetName() {
  DCHECK(shill_dictionary_);
  std::string name;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kNameProperty, &name);
  return name;
}

}  // namespace

std::unique_ptr<base::DictionaryValue> TranslateShillServiceToONCPart(
    const base::DictionaryValue& shill_dictionary,
    ::onc::ONCSource onc_source,
    const OncValueSignature* onc_signature,
    const NetworkState* network_state) {
  CHECK(onc_signature != NULL);

  ShillToONCTranslator translator(shill_dictionary, onc_source, *onc_signature,
                                  network_state);
  return translator.CreateTranslatedONCObject();
}

}  // namespace onc
}  // namespace chromeos
