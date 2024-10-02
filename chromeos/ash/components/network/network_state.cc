// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_state.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_config.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "net/http/http_status_code.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

namespace network_config {
namespace mojom = ::chromeos::network_config::mojom;
}

// TODO(tbarzic): Add payment portal method values to shill/dbus-constants.
constexpr char kPaymentPortalMethodPost[] = "POST";

// |dict| may be an empty value, in which case return an empty string.
std::string GetStringFromDictionary(
    const std::optional<base::Value::Dict>& dict,
    const char* key) {
  const std::string* stringp =
      dict.has_value() ? dict->FindString(key) : nullptr;
  return stringp ? *stringp : std::string();
}

bool IsValidConnectionState(const std::string& connection_state) {
  return connection_state == shill::kStateIdle ||
         connection_state == shill::kStateAssociation ||
         connection_state == shill::kStateConfiguration ||
         connection_state == shill::kStateReady ||
         connection_state == shill::kStateNoConnectivity ||
         connection_state == shill::kStateRedirectFound ||
         connection_state == shill::kStatePortalSuspected ||
         connection_state == shill::kStateOnline ||
         connection_state == shill::kStateFailure ||
         connection_state == shill::kStateDisconnecting;
}

}  // namespace

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path) {}

NetworkState::~NetworkState() = default;

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  // Keep care that these properties are the same as in |GetProperties|.
  if (ManagedStatePropertyChanged(key, value)) {
    return true;
  }
  if (key == shill::kSignalStrengthProperty) {
    int signal_strength = signal_strength_;
    if (!GetIntegerValue(key, value, &signal_strength)) {
      return false;
    }
    if (signal_strength_ > 0 && abs(signal_strength - signal_strength_) <
                                    kSignalStrengthChangeThreshold) {
      return false;
    }
    signal_strength_ = signal_strength;
    return true;
  } else if (key == shill::kWifiSignalStrengthRssiProperty) {
    int rssi = rssi_;
    if (!GetIntegerValue(key, value, &rssi)) {
      return false;
    }
    if (rssi == rssi_) {
      return false;
    }
    rssi_ = rssi;
    return true;
  } else if (key == shill::kStateProperty) {
    std::string connection_state;
    if (!GetStringValue(key, value, &connection_state)) {
      return false;
    }
    SetConnectionState(connection_state);
    return true;
  } else if (key == shill::kVisibleProperty) {
    return GetBooleanValue(key, value, &visible_);
  } else if (key == shill::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == shill::kErrorProperty) {
    std::string error;
    if (!GetStringValue(key, value, &error)) {
      return false;
    }
    if (ErrorIsValid(error)) {
      last_error_ = error;
    }
    return true;
  } else if (key == shill::kWifiFrequency) {
    return GetIntegerValue(key, value, &frequency_);
  } else if (key == shill::kActivationTypeProperty) {
    return GetStringValue(key, value, &activation_type_);
  } else if (key == shill::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == shill::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == shill::kCellularAllowRoamingProperty) {
    return GetBooleanValue(key, value, &allow_roaming_);
  } else if (key == shill::kPaymentPortalProperty) {
    const base::Value::Dict* value_dict = value.GetIfDict();
    if (!value_dict) {
      return false;
    }
    const std::string* portal_url_value =
        value_dict->FindString(shill::kPaymentPortalURL);
    if (!portal_url_value) {
      return false;
    }
    payment_url_ = *portal_url_value;
    // If payment portal uses post method, set up post data.
    const std::string* portal_method_value =
        value_dict->FindString(shill::kPaymentPortalMethod);
    payment_method_ =
        portal_method_value ? *portal_method_value : std::string();

    const std::string* portal_post_data_value =
        value_dict->FindString(shill::kPaymentPortalPostData);
    if (payment_method_ == kPaymentPortalMethodPost && portal_post_data_value) {
      payment_post_data_ = *portal_post_data_value;
    }
    return true;
  } else if (key == shill::kSecurityClassProperty) {
    return GetStringValue(key, value, &security_class_);
  } else if (key == shill::kEapMethodProperty) {
    return GetStringValue(key, value, &eap_method_);
  } else if (key == shill::kEapKeyMgmtProperty) {
    return GetStringValue(key, value, &eap_key_mgmt_);
  } else if (key == shill::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &network_technology_);
  } else if (key == shill::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == shill::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == shill::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kWifiHexSsid) {
    std::string ssid_hex;
    if (!GetStringValue(key, value, &ssid_hex)) {
      return false;
    }
    raw_ssid_.clear();
    return base::HexStringToBytes(ssid_hex, &raw_ssid_);
  } else if (key == shill::kWifiBSsid) {
    return GetStringValue(key, value, &bssid_);
  } else if (key == shill::kPriorityProperty) {
    return GetIntegerValue(key, value, &priority_);
  } else if (key == shill::kWifiHiddenSsid) {
    return GetBooleanValue(key, value, &hidden_ssid_);
  } else if (key == shill::kPasspointIDProperty) {
    return GetStringValue(key, value, &passpoint_id_);
  } else if (key == shill::kMeteredProperty) {
    return GetBooleanValue(key, value, &metered_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  } else if (key == shill::kIccidProperty) {
    return GetStringValue(key, value, &iccid_);
  } else if (key == shill::kEidProperty) {
    return GetStringValue(key, value, &eid_);
  } else if (key == shill::kProxyConfigProperty) {
    const std::string* proxy_config_str = value.GetIfString();
    if (!proxy_config_str) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }

    if ((*proxy_config_str).empty()) {
      proxy_config_.reset();
      return true;
    }
    std::optional<base::Value::Dict> proxy_config =
        chromeos::onc::ReadDictionaryFromJson(*proxy_config_str);
    if (!proxy_config.has_value()) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      proxy_config_.reset();
    } else {
      proxy_config_ = std::move(proxy_config.value());
    }
    return true;
  } else if (key == shill::kProviderProperty) {
    const std::string* vpn_provider_type =
        value.is_dict() ? value.GetDict().FindString(shill::kTypeProperty)
                        : nullptr;
    if (!vpn_provider_type) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }
    std::string vpn_provider_id;
    if (*vpn_provider_type == shill::kProviderThirdPartyVpn ||
        *vpn_provider_type == shill::kProviderArcVpn) {
      // If the network uses a third-party or Arc VPN provider,
      // |shill::kHostProperty| contains the extension ID or Arc package name.
      const std::string* host =
          value.GetDict().FindString(shill::kHostProperty);
      if (!host) {
        NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
        return false;
      }
      vpn_provider_id = *host;
    }
    SetVpnProvider(vpn_provider_id, *vpn_provider_type);
    return true;
  } else if (key == shill::kUIDataProperty) {
    std::unique_ptr<NetworkUIData> ui_data =
        shill_property_util::GetUIDataFromValue(value);
    if (!ui_data) {
      return false;
    }
    onc_source_ = ui_data->onc_source();
    return true;
  } else if (key == shill::kProbeUrlProperty) {
    std::string probe_url_string;
    if (!GetStringValue(key, value, &probe_url_string)) {
      return false;
    }
    probe_url_ = GURL(probe_url_string);
    return true;
  } else if (key == shill::kUplinkSpeedPropertyKbps) {
    uint32_t max_uplink_speed_kbps;
    if (!GetUInt32Value(key, value, &max_uplink_speed_kbps)) {
      return false;
    }
    if (max_uplink_speed_kbps_.has_value() &&
        max_uplink_speed_kbps == max_uplink_speed_kbps_.value()) {
      return false;
    }
    max_uplink_speed_kbps_ = max_uplink_speed_kbps;
    return true;
  } else if (key == shill::kDownlinkSpeedPropertyKbps) {
    uint32_t max_downlink_speed_kbps;
    if (!GetUInt32Value(key, value, &max_downlink_speed_kbps)) {
      return false;
    }
    if (max_downlink_speed_kbps_.has_value() &&
        max_downlink_speed_kbps == max_downlink_speed_kbps_.value()) {
      return false;
    }
    max_downlink_speed_kbps_ = max_downlink_speed_kbps;
    return true;
  } else if (key == shill::kNetworkConfigProperty) {
    network_config_ = NetworkConfig::ParseFromServicePropertyValue(value);
    return true;
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(
    const base::Value::Dict& properties) {
  NET_LOG(EVENT) << "InitialPropertiesReceived: " << NetworkId(this)
                 << " State: " << connection_state_ << " Visible: " << visible_;
  if (!properties.contains(shill::kTypeProperty)) {
    NET_LOG(ERROR) << "NetworkState has no type: " << NetworkId(this);
    return false;
  }

  // By convention, all visible WiFi networks have a SignalStrength > 0.
  if (type() == shill::kTypeWifi && visible() && signal_strength_ <= 0) {
    signal_strength_ = 1;
  }

  // Any change to connection state or portal properties will trigger a complete
  // property update, so we update captive portal state here.
  UpdateCaptivePortalState(properties);

  // Ensure that the network has a valid name.
  return UpdateName(properties);
}

void NetworkState::GetStateProperties(base::Value::Dict* dictionary) const {
  ManagedState::GetStateProperties(dictionary);

  // Properties shared by all types.
  dictionary->Set(shill::kGuidProperty, guid());
  dictionary->Set(shill::kSecurityClassProperty, security_class());
  dictionary->Set(shill::kProfileProperty, profile_path());
  dictionary->Set(shill::kPriorityProperty, priority_);

  if (visible()) {
    dictionary->Set(shill::kStateProperty, connection_state());
  }
  if (!device_path().empty()) {
    dictionary->Set(shill::kDeviceProperty, device_path());
  }

  // VPN properties.
  if (NetworkTypePattern::VPN().MatchesType(type()) && vpn_provider()) {
    // Shill sends VPN provider properties in a nested dictionary. |dictionary|
    // must replicate that nested structure.
    std::string provider_type = vpn_provider()->type;
    auto provider_property =
        base::Value::Dict().Set(shill::kTypeProperty, provider_type);
    if (provider_type == shill::kProviderThirdPartyVpn ||
        provider_type == shill::kProviderArcVpn) {
      provider_property.Set(shill::kHostProperty, vpn_provider()->id);
    }
    dictionary->Set(shill::kProviderProperty, std::move(provider_property));
  }

  // Tether properties
  if (NetworkTypePattern::Tether().MatchesType(type())) {
    dictionary->Set(kTetherBatteryPercentage, battery_percentage());
    dictionary->Set(kTetherCarrier, tether_carrier());
    dictionary->Set(kTetherHasConnectedToHost, tether_has_connected_to_host());
    dictionary->Set(kTetherSignalStrength, signal_strength());

    // All Tether networks are connectable.
    dictionary->Set(shill::kConnectableProperty, connectable());

    // Tether networks do not share some of the wireless/mobile properties added
    // below; exit early to avoid having these properties applied.
    return;
  }

  // Wireless properties
  if (!NetworkTypePattern::Wireless().MatchesType(type())) {
    return;
  }

  if (visible()) {
    dictionary->Set(shill::kConnectableProperty, connectable());
    dictionary->Set(shill::kSignalStrengthProperty, signal_strength());
  }

  // Wifi properties
  if (NetworkTypePattern::WiFi().MatchesType(type())) {
    dictionary->Set(shill::kWifiBSsid, bssid_);
    dictionary->Set(shill::kEapMethodProperty, eap_method());
    dictionary->Set(shill::kWifiFrequency, frequency_);
    dictionary->Set(shill::kWifiHexSsid, GetHexSsid());
  }

  // Mobile properties
  if (NetworkTypePattern::Mobile().MatchesType(type())) {
    dictionary->Set(shill::kNetworkTechnologyProperty, network_technology());
    dictionary->Set(shill::kActivationStateProperty, activation_state());
    dictionary->Set(shill::kRoamingStateProperty, roaming_);
    dictionary->Set(shill::kOutOfCreditsProperty, cellular_out_of_credits());
  }

  // Cellular properties
  if (NetworkTypePattern::Cellular().MatchesType(type())) {
    dictionary->Set(shill::kIccidProperty, iccid());
    dictionary->Set(shill::kEidProperty, eid());
  }
}

bool NetworkState::IsActive() const {
  return IsConnectingOrConnected() ||
         activation_state() == shill::kActivationStateActivating;
}

void NetworkState::IPConfigPropertiesChanged(
    const base::Value::Dict& properties) {
  if (properties.empty()) {
    ipv4_config_.reset();
    return;
  }
  ipv4_config_ = properties.Clone();
}

std::string NetworkState::GetIpAddress() const {
  return GetStringFromDictionary(ipv4_config_, shill::kAddressProperty);
}

std::string NetworkState::GetGateway() const {
  return GetStringFromDictionary(ipv4_config_, shill::kGatewayProperty);
}

GURL NetworkState::GetWebProxyAutoDiscoveryUrl() const {
  std::string url = GetStringFromDictionary(
      ipv4_config_, shill::kWebProxyAutoDiscoveryUrlProperty);
  if (url.empty()) {
    return GURL();
  }
  GURL gurl(url);
  if (!gurl.is_valid()) {
    NET_LOG(ERROR) << "Invalid WebProxyAutoDiscoveryUrl: " << NetworkId(this)
                   << ": " << url;
    return GURL();
  }
  return gurl;
}

std::string NetworkState::GetVpnProviderType() const {
  return vpn_provider_ ? vpn_provider_->type : std::string();
}

bool NetworkState::RequiresActivation() const {
  return type() == shill::kTypeCellular &&
         (activation_state() == shill::kActivationStateNotActivated ||
          activation_state() == shill::kActivationStatePartiallyActivated);
}

bool NetworkState::SecurityRequiresPassphraseOnly() const {
  return type() == shill::kTypeWifi &&
         (security_class_ == shill::kSecurityClassPsk ||
          security_class_ == shill::kSecurityClassWep);
}

const std::string& NetworkState::GetError() const {
  return last_error_;
}

void NetworkState::ClearError() {
  last_error_.clear();
}

std::string NetworkState::connection_state() const {
  if (!visible()) {
    return shill::kStateIdle;
  }

  return connection_state_;
}

void NetworkState::SetConnectionState(const std::string& connection_state) {
  DCHECK(IsValidConnectionState(connection_state)) << connection_state;

  if (connection_state == connection_state_) {
    return;
  }
  const std::string prev_connection_state = connection_state_;
  connection_state_ = connection_state;
  if (StateIsConnected(connection_state_) ||
      StateIsConnecting(prev_connection_state)) {
    // If connected or previously connecting, clear |connect_requested_|.
    connect_requested_ = false;
  } else if (StateIsConnected(prev_connection_state) &&
             StateIsConnecting(connection_state_)) {
    // If transitioning from a connected state to a connecting state, set
    // |connect_requested_| so that the UI knows the connecting state is
    // important (i.e. not a normal auto connect).
    connect_requested_ = true;
  }

  // TODO(b/336931625): Polish the relationship between |portal_state_|
  // and |connection_state_|.
  if (portal_state_ == PortalState::kUnknown &&
      connection_state_ == shill::kStateOnline) {
    portal_state_ = PortalState::kOnline;
  }
}

bool NetworkState::IsManagedByPolicy() const {
  return onc_source_ == ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY ||
         onc_source_ == ::onc::ONCSource::ONC_SOURCE_USER_POLICY;
}

bool NetworkState::IndicateRoaming() const {
  return type() == shill::kTypeCellular &&
         roaming_ == shill::kRoamingStateRoaming && !provider_requires_roaming_;
}

bool NetworkState::IsDynamicWep() const {
  return security_class_ == shill::kSecurityClassWep &&
         eap_key_mgmt_ == shill::kKeyManagementIEEE8021X;
}

bool NetworkState::IsConnectedState() const {
  return visible() && StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return visible() &&
         (connect_requested_ || StateIsConnecting(connection_state_));
}

bool NetworkState::IsConnectingOrConnected() const {
  return visible() &&
         (connect_requested_ || StateIsConnecting(connection_state_) ||
          StateIsConnected(connection_state_));
}

bool NetworkState::IsOnline() const {
  return connection_state() == shill::kStateOnline;
}

bool NetworkState::IsInProfile() const {
  // kTypeEthernetEap is always saved. We need this check because it does
  // not show up in the visible list, but its properties may not be available
  // when it first shows up in ServiceCompleteList. See crbug.com/355117.
  return !profile_path_.empty() || type() == shill::kTypeEthernetEap;
}

bool NetworkState::IsNonProfileType() const {
  return type() == kTypeTether || IsNonShillCellularNetwork();
}

bool NetworkState::IsPrivate() const {
  return !profile_path_.empty() &&
         profile_path_ != NetworkProfileHandler::GetSharedProfilePath();
}

bool NetworkState::IsNonShillCellularNetwork() const {
  return type() == shill::kTypeCellular &&
         cellular_utils::IsStubCellularServicePath(path());
}

bool NetworkState::IsSecure() const {
  return !security_class_.empty() &&
         security_class_ != shill::kSecurityClassNone;
}

std::string NetworkState::GetHexSsid() const {
  return base::HexEncode(raw_ssid());
}

std::string NetworkState::GetDnsServersAsString() const {
  const base::Value::List* list =
      ipv4_config_.has_value()
          ? ipv4_config_->FindList(shill::kNameServersProperty)
          : nullptr;
  if (!list) {
    return std::string();
  }
  std::string result;
  for (const auto& v : *list) {
    if (!result.empty()) {
      result += ",";
    }
    result += v.GetString();
  }
  return result;
}

std::string NetworkState::GetNetmask() const {
  int prefixlen = ipv4_config_->FindInt(shill::kPrefixlenProperty).value_or(-1);
  return network_util::PrefixLengthToNetmask(prefixlen);
}

std::string NetworkState::GetSpecifier() const {
  if (!update_received()) {
    NET_LOG(ERROR) << "GetSpecifier called before update: " << NetworkId(this);
    return std::string();
  }
  if (type() == shill::kTypeWifi) {
    return name() + "_" + security_class_;
  }
  if (type() == shill::kTypeCellular && !iccid().empty()) {
    return iccid();
  }
  if (!name().empty()) {
    return type() + "_" + name();
  }
  return type();  // For unnamed networks, i.e. Ethernet.
}

void NetworkState::SetGuid(const std::string& guid) {
  guid_ = guid;
}

network_config::mojom::ActivationStateType
NetworkState::GetMojoActivationState() const {
  using network_config::mojom::ActivationStateType;
  if (activation_state_.empty()) {
    return ActivationStateType::kUnknown;
  }
  if (activation_state_ == shill::kActivationStateActivated) {
    return ActivationStateType::kActivated;
  }
  if (activation_state_ == shill::kActivationStateActivating) {
    return ActivationStateType::kActivating;
  }
  if (activation_state_ == shill::kActivationStateNotActivated) {
    return ActivationStateType::kNotActivated;
  }
  if (activation_state_ == shill::kActivationStatePartiallyActivated) {
    return ActivationStateType::kPartiallyActivated;
  }
  NET_LOG(ERROR) << "Unexpected shill activation state: " << activation_state_;
  return ActivationStateType::kUnknown;
}

network_config::mojom::SecurityType NetworkState::GetMojoSecurity() const {
  using network_config::mojom::SecurityType;
  if (!IsSecure()) {
    return SecurityType::kNone;
  }
  if (IsDynamicWep()) {
    return SecurityType::kWep8021x;
  }

  if (security_class_ == shill::kSecurityClassWep) {
    return SecurityType::kWepPsk;
  }
  if (security_class_ == shill::kSecurityClassPsk) {
    return SecurityType::kWpaPsk;
  }
  if (security_class_ == shill::kSecurityClass8021x) {
    return SecurityType::kWpaEap;
  }
  NET_LOG(ERROR) << "Unsupported shill security class: " << security_class_;
  return SecurityType::kNone;
}

NetworkState::NetworkTechnologyType NetworkState::GetNetworkTechnologyType()
    const {
  const std::string& network_type = type();
  if (network_type == shill::kTypeCellular) {
    return NetworkTechnologyType::kCellular;
  }
  if (network_type == shill::kTypeEthernet) {
    return NetworkTechnologyType::kEthernet;
  }
  if (network_type == shill::kTypeEthernetEap) {
    return NetworkTechnologyType::kEthernet;
  }
  if (network_type == kTypeTether) {
    return NetworkTechnologyType::kTether;
  }
  if (network_type == shill::kTypeVPN) {
    return NetworkTechnologyType::kVPN;
  }
  if (network_type == shill::kTypeWifi) {
    return NetworkTechnologyType::kWiFi;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown network type: " << network_type;
  return NetworkTechnologyType::kUnknown;
}

// static
bool NetworkState::StateIsConnected(const std::string& connection_state) {
  return (connection_state == shill::kStateReady ||
          connection_state == shill::kStateOnline ||
          StateIsPortalled(connection_state));
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == shill::kStateAssociation ||
          connection_state == shill::kStateConfiguration);
}

// static
bool NetworkState::StateIsPortalled(const std::string& connection_state) {
  return (connection_state == shill::kStateNoConnectivity ||
          connection_state == shill::kStateRedirectFound ||
          connection_state == shill::kStatePortalSuspected);
}

// static
bool NetworkState::ErrorIsValid(const std::string& error) {
  return !error.empty() && error != shill::kErrorNoFailure;
}

// static
std::unique_ptr<NetworkState> NetworkState::CreateNonShillCellularNetwork(
    const std::string& iccid,
    const std::string& eid,
    const std::string& guid,
    bool is_managed,
    const std::string& cellular_device_path) {
  std::string path = cellular_utils::GenerateStubCellularServicePath(iccid);
  auto new_state = std::make_unique<NetworkState>(path);
  new_state->set_type(shill::kTypeCellular);
  new_state->set_update_received();
  new_state->set_visible(true);
  new_state->device_path_ = cellular_device_path;
  new_state->iccid_ = iccid;
  new_state->eid_ = eid;
  new_state->guid_ = guid;
  if (is_managed) {
    new_state->onc_source_ = ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY;
  }
  new_state->activation_state_ = shill::kActivationStateActivated;
  return new_state;
}

// Private methods.

bool NetworkState::UpdateName(const base::Value::Dict& properties) {
  std::string updated_name =
      shill_property_util::GetNameFromProperties(path(), properties);
  if (updated_name != name()) {
    set_name(updated_name);
    return true;
  }
  return false;
}

void NetworkState::UpdateCaptivePortalState(
    const base::Value::Dict& properties) {
  if (!IsConnectedState()) {
    // Unconnected networks are in an unknown portal state and should not
    // update histograms.
    portal_state_ = PortalState::kUnknown;
    return;
  }

  if (connection_state_ == shill::kStateNoConnectivity) {
    portal_state_ = PortalState::kNoInternet;
  } else if (connection_state_ == shill::kStateRedirectFound) {
    portal_state_ = PortalState::kPortal;
  } else if (connection_state_ == shill::kStatePortalSuspected) {
    portal_state_ = PortalState::kPortalSuspected;
  } else if (connection_state_ == shill::kStateOnline) {
    portal_state_ = PortalState::kOnline;
  } else {
    NET_LOG(ERROR) << "Unexpected captive portal state for: " << NetworkId(this)
                   << " = " << portal_state_;
    portal_state_ = PortalState::kUnknown;
  }

  base::UmaHistogramEnumeration("Network.CaptivePortalResult", portal_state_);
  if (portal_state_ != PortalState::kOnline) {
    NET_LOG(EVENT) << "Shill captive portal state for: " << NetworkId(this)
                   << " = " << portal_state_;
  }
}

void NetworkState::SetVpnProvider(const std::string& id,
                                  const std::string& type) {
  // |type| is required but |id| is only set for ThirdParty and Arc VPNs.
  if (type.empty()) {
    vpn_provider_ = nullptr;
    return;
  }
  if (!vpn_provider_) {
    vpn_provider_ = std::make_unique<VpnProviderInfo>();
  }
  vpn_provider_->id = id;
  vpn_provider_->type = type;
}

std::ostream& operator<<(std::ostream& out,
                         const NetworkState::PortalState state) {
  using State = NetworkState::PortalState;
  switch (state) {
#define PRINT(s)    \
  case State::k##s: \
    return out << #s;
    PRINT(Unknown)
    PRINT(Online)
    PRINT(PortalSuspected)
    PRINT(Portal)
    PRINT(NoInternet)
#undef PRINT
  }

  return out << "PortalState("
             << static_cast<std::underlying_type_t<State>>(state) << ")";
}

std::ostream& operator<<(std::ostream& out,
                         const NetworkState::NetworkTechnologyType type) {
  using Type = NetworkState::NetworkTechnologyType;
  switch (type) {
#define PRINT(s)   \
  case Type::k##s: \
    return out << #s;
    PRINT(Cellular)
    PRINT(Ethernet)
    PRINT(EthernetEap)
    PRINT(WiFi)
    PRINT(Tether)
    PRINT(VPN)
    PRINT(Unknown)
#undef PRINT
  }

  return out << "NetworkTechnologyType("
             << static_cast<std::underlying_type_t<Type>>(type) << ")";
}

}  // namespace ash
