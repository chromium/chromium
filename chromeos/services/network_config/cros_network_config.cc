// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/cros_network_config.h"

#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config_mojom_traits.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "components/user_manager/user_manager.h"
#include "net/base/ip_address.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using user_manager::UserManager;

namespace chromeos {
namespace network_config {

namespace {

// Error strings from networking_private_api.cc. TODO(1004434): Enumerate
// these in mojo.
const char kErrorAccessToSharedConfig[] = "Error.CannotChangeSharedConfig";
const char kErrorInvalidONCConfiguration[] = "Error.InvalidONCConfiguration";
const char kErrorNetworkUnavailable[] = "Error.NetworkUnavailable";
const char kErrorNotReady[] = "Error.NotReady";

std::string ShillToOnc(const std::string& shill_string,
                       const onc::StringTranslationEntry table[]) {
  std::string onc_string;
  if (!shill_string.empty())
    onc::TranslateStringToONC(table, shill_string, &onc_string);
  return onc_string;
}

mojom::NetworkType NetworkPatternToMojo(NetworkTypePattern type) {
  if (type.Equals(NetworkTypePattern::Cellular()))
    return mojom::NetworkType::kCellular;
  if (type.MatchesPattern(NetworkTypePattern::EthernetOrEthernetEAP()))
    return mojom::NetworkType::kEthernet;
  if (type.Equals(NetworkTypePattern::Tether()))
    return mojom::NetworkType::kTether;
  if (type.Equals(NetworkTypePattern::VPN()))
    return mojom::NetworkType::kVPN;
  if (type.Equals(NetworkTypePattern::WiFi()))
    return mojom::NetworkType::kWiFi;
  NOTREACHED() << "Unsupported network type: " << type.ToDebugString();
  return mojom::NetworkType::kAll;  // Unsupported
}

mojom::NetworkType ShillTypeToMojo(const std::string& shill_type) {
  return NetworkPatternToMojo(NetworkTypePattern::Primitive(shill_type));
}

mojom::NetworkType OncTypeToMojo(const std::string& onc_type) {
  return NetworkPatternToMojo(NetworkTypePattern::Primitive(
      network_util::TranslateONCTypeToShill(onc_type)));
}

NetworkTypePattern MojoTypeToPattern(mojom::NetworkType type) {
  switch (type) {
    case mojom::NetworkType::kAll:
      return NetworkTypePattern::Default();
    case mojom::NetworkType::kCellular:
      return NetworkTypePattern::Cellular();
    case mojom::NetworkType::kEthernet:
      return NetworkTypePattern::Ethernet();
    case mojom::NetworkType::kMobile:
      return NetworkTypePattern::Mobile();
    case mojom::NetworkType::kTether:
      return NetworkTypePattern::Tether();
    case mojom::NetworkType::kVPN:
      return NetworkTypePattern::VPN();
    case mojom::NetworkType::kWireless:
      return NetworkTypePattern::Wireless();
    case mojom::NetworkType::kWiFi:
      return NetworkTypePattern::WiFi();
  }
  NOTREACHED();
  return NetworkTypePattern::Default();
}

std::string MojoNetworkTypeToOnc(mojom::NetworkType type) {
  switch (type) {
    case mojom::NetworkType::kAll:
    case mojom::NetworkType::kMobile:
    case mojom::NetworkType::kWireless:
      break;  // Not supported
    case mojom::NetworkType::kCellular:
      return ::onc::network_type::kCellular;
    case mojom::NetworkType::kEthernet:
      return ::onc::network_type::kEthernet;
    case mojom::NetworkType::kTether:
      return ::onc::network_type::kTether;
    case mojom::NetworkType::kVPN:
      return ::onc::network_type::kVPN;
    case mojom::NetworkType::kWiFi:
      return ::onc::network_type::kWiFi;
  }
  NOTREACHED() << "Unsupported mojo to ONC type: " << type;
  return std::string();
}

mojom::ConnectionStateType GetMojoConnectionStateType(
    const NetworkState* network) {
  if (network->IsConnectedState()) {
    if (network->IsCaptivePortal())
      return mojom::ConnectionStateType::kPortal;
    if (network->IsOnline())
      return mojom::ConnectionStateType::kOnline;
    return mojom::ConnectionStateType::kConnected;
  }
  if (network->IsConnectingState())
    return mojom::ConnectionStateType::kConnecting;
  return mojom::ConnectionStateType::kNotConnected;
}

mojom::ConnectionStateType GetConnectionState(const NetworkState* network,
                                              bool technology_enabled) {
  // If a network technology is not enabled, always use NotConnected as the
  // connection state to avoid any edge cases during device enable/disable.
  return technology_enabled ? GetMojoConnectionStateType(network)
                            : mojom::ConnectionStateType::kNotConnected;
}

std::string MojoSecurityTypeToOnc(mojom::SecurityType security_type) {
  switch (security_type) {
    case mojom::SecurityType::kNone:
      return ::onc::wifi::kSecurityNone;
    case mojom::SecurityType::kWep8021x:
      return ::onc::wifi::kWEP_8021X;
    case mojom::SecurityType::kWepPsk:
      return ::onc::wifi::kWEP_PSK;
    case mojom::SecurityType::kWpaEap:
      return ::onc::wifi::kWPA_EAP;
    case mojom::SecurityType::kWpaPsk:
      return ::onc::wifi::kWPA_PSK;
  }
  NOTREACHED() << "Unsupported mojo to ONC type: " << security_type;
  return std::string();
}

mojom::VpnType OncVpnTypeToMojo(const std::string& onc_vpn_type) {
  if (onc_vpn_type == ::onc::vpn::kTypeL2TP_IPsec)
    return mojom::VpnType::kL2TPIPsec;
  if (onc_vpn_type == ::onc::vpn::kOpenVPN)
    return mojom::VpnType::kOpenVPN;
  if (onc_vpn_type == ::onc::vpn::kThirdPartyVpn)
    return mojom::VpnType::kExtension;
  if (onc_vpn_type == ::onc::vpn::kArcVpn)
    return mojom::VpnType::kArc;
  NOTREACHED() << "Unsupported ONC VPN type: " << onc_vpn_type;
  return mojom::VpnType::kOpenVPN;
}

std::string MojoVpnTypeToOnc(mojom::VpnType mojo_vpn_type) {
  switch (mojo_vpn_type) {
    case mojom::VpnType::kL2TPIPsec:
      return ::onc::vpn::kTypeL2TP_IPsec;
    case mojom::VpnType::kOpenVPN:
      return ::onc::vpn::kOpenVPN;
    case mojom::VpnType::kExtension:
      return ::onc::vpn::kThirdPartyVpn;
    case mojom::VpnType::kArc:
      return ::onc::vpn::kArcVpn;
  }
  NOTREACHED();
  return ::onc::vpn::kOpenVPN;
}

mojom::DeviceStateType GetMojoDeviceStateType(
    NetworkStateHandler::TechnologyState technology_state) {
  switch (technology_state) {
    case NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      return mojom::DeviceStateType::kUnavailable;
    case NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
      return mojom::DeviceStateType::kUninitialized;
    case NetworkStateHandler::TECHNOLOGY_AVAILABLE:
      return mojom::DeviceStateType::kDisabled;
    case NetworkStateHandler::TECHNOLOGY_DISABLING:
      // TODO(jonmann): Add a DeviceStateType::kDisabling.
      return mojom::DeviceStateType::kDisabled;
    case NetworkStateHandler::TECHNOLOGY_ENABLING:
      return mojom::DeviceStateType::kEnabling;
    case NetworkStateHandler::TECHNOLOGY_ENABLED:
      return mojom::DeviceStateType::kEnabled;
    case NetworkStateHandler::TECHNOLOGY_PROHIBITED:
      return mojom::DeviceStateType::kProhibited;
  }
  NOTREACHED();
  return mojom::DeviceStateType::kUnavailable;
}

mojom::OncSource GetMojoOncSource(const NetworkState* network) {
  ::onc::ONCSource source = network->onc_source();
  switch (source) {
    case ::onc::ONC_SOURCE_UNKNOWN:
    case ::onc::ONC_SOURCE_NONE:
      if (!network->IsInProfile())
        return mojom::OncSource::kNone;
      return network->IsPrivate() ? mojom::OncSource::kUser
                                  : mojom::OncSource::kDevice;
    case ::onc::ONC_SOURCE_USER_IMPORT:
      return mojom::OncSource::kUser;
    case ::onc::ONC_SOURCE_DEVICE_POLICY:
      return mojom::OncSource::kDevicePolicy;
    case ::onc::ONC_SOURCE_USER_POLICY:
      return mojom::OncSource::kUserPolicy;
  }
  NOTREACHED();
  return mojom::OncSource::kNone;
}

const std::string& GetVpnProviderName(
    const std::vector<mojom::VpnProviderPtr>& vpn_providers,
    const std::string& provider_id) {
  for (const mojom::VpnProviderPtr& provider : vpn_providers) {
    if (provider->provider_id == provider_id)
      return provider->provider_name;
  }
  return base::EmptyString();
}

mojom::NetworkStatePropertiesPtr NetworkStateToMojo(
    NetworkStateHandler* network_state_handler,
    const std::vector<mojom::VpnProviderPtr>& vpn_providers,
    const NetworkState* network) {
  mojom::NetworkType type = ShillTypeToMojo(network->type());
  if (type == mojom::NetworkType::kAll) {
    NET_LOG(ERROR) << "Unexpected network type: " << network->type()
                   << " GUID: " << network->guid();
    return nullptr;
  }

  auto result = mojom::NetworkStateProperties::New();
  result->type = type;
  result->connectable = network->connectable();
  if (type == mojom::NetworkType::kCellular) {
    // Ensure that a cellular network that has a locked sim state or is scanning
    // is not connectable.
    const DeviceState* device =
        network_state_handler->GetDeviceState(network->device_path());
    if (!device) {
      // When a device is removed (e.g. cellular modem unplugged) it's possible
      // for the device object to disappear before networks on that device
      // are cleaned up.  This fixes crbug/1001687.
      NET_LOG(DEBUG) << "Cellular is not available.";
      return nullptr;
    }

    if (device->IsSimLocked() || device->scanning())
      result->connectable = false;
  }
  result->connect_requested = network->connect_requested();
  bool technology_enabled = network->Matches(NetworkTypePattern::VPN()) ||
                            network_state_handler->IsTechnologyEnabled(
                                NetworkTypePattern::Primitive(network->type()));
  result->connection_state = GetConnectionState(network, technology_enabled);
  if (!network->GetError().empty())
    result->error_state = network->GetError();
  result->guid = network->guid();
  result->name = network->name();
  result->priority = network->priority();
  result->prohibited_by_policy = network->blocked_by_policy();
  result->source = GetMojoOncSource(network);

  // NetworkHandler and UIProxyConfigService may not exist in tests.
  UIProxyConfigService* ui_proxy_config_service =
      NetworkHandler::IsInitialized() &&
              NetworkHandler::Get()->has_ui_proxy_config_service()
          ? NetworkHandler::Get()->ui_proxy_config_service()
          : nullptr;
  result->proxy_mode =
      ui_proxy_config_service
          ? mojom::ProxyMode(
                ui_proxy_config_service->ProxyModeForNetwork(network))
          : mojom::ProxyMode::kDirect;

  const NetworkState::CaptivePortalProviderInfo* captive_portal_provider =
      network->captive_portal_provider();
  if (captive_portal_provider) {
    auto mojo_captive_portal_provider = mojom::CaptivePortalProvider::New();
    mojo_captive_portal_provider->id = captive_portal_provider->id;
    mojo_captive_portal_provider->name = captive_portal_provider->name;
    result->captive_portal_provider = std::move(mojo_captive_portal_provider);
  }

  switch (type) {
    case mojom::NetworkType::kCellular: {
      auto cellular = mojom::CellularStateProperties::New();
      cellular->activation_state = network->GetMojoActivationState();
      cellular->network_technology = ShillToOnc(network->network_technology(),
                                                onc::kNetworkTechnologyTable);
      cellular->roaming = network->IndicateRoaming();
      cellular->signal_strength = network->signal_strength();

      const DeviceState* cellular_device =
          network_state_handler->GetDeviceState(network->device_path());
      cellular->sim_locked = cellular_device->IsSimLocked();
      result->type_state =
          mojom::NetworkTypeStateProperties::NewCellular(std::move(cellular));
      break;
    }
    case mojom::NetworkType::kEthernet: {
      auto ethernet = mojom::EthernetStateProperties::New();
      ethernet->authentication = network->type() == shill::kTypeEthernetEap
                                     ? mojom::AuthenticationType::k8021x
                                     : mojom::AuthenticationType::kNone;
      result->type_state =
          mojom::NetworkTypeStateProperties::NewEthernet(std::move(ethernet));
      break;
    }
    case mojom::NetworkType::kTether: {
      auto tether = mojom::TetherStateProperties::New();
      tether->battery_percentage = network->battery_percentage();
      tether->carrier = network->tether_carrier();
      tether->has_connected_to_host = network->tether_has_connected_to_host();
      tether->signal_strength = network->signal_strength();
      result->type_state =
          mojom::NetworkTypeStateProperties::NewTether(std::move(tether));
      break;
    }
    case mojom::NetworkType::kVPN: {
      auto vpn = mojom::VPNStateProperties::New();
      const NetworkState::VpnProviderInfo* vpn_provider =
          network->vpn_provider();
      if (vpn_provider) {
        vpn->type = OncVpnTypeToMojo(
            ShillToOnc(vpn_provider->type, onc::kVPNTypeTable));
        vpn->provider_id = vpn_provider->id;
        vpn->provider_name =
            GetVpnProviderName(vpn_providers, vpn_provider->id);
      }
      result->type_state =
          mojom::NetworkTypeStateProperties::NewVpn(std::move(vpn));
      break;
    }
    case mojom::NetworkType::kWiFi: {
      auto wifi = mojom::WiFiStateProperties::New();
      wifi->bssid = network->bssid();
      wifi->frequency = network->frequency();
      wifi->hex_ssid = network->GetHexSsid();
      wifi->security = network->GetMojoSecurity();
      wifi->signal_strength = network->signal_strength();
      wifi->ssid = network->name();
      result->type_state =
          mojom::NetworkTypeStateProperties::NewWifi(std::move(wifi));
      break;
    }
    case mojom::NetworkType::kAll:
    case mojom::NetworkType::kMobile:
    case mojom::NetworkType::kWireless:
      NOTREACHED() << "NetworkStateProperties can not be of type: " << type;
      break;
  }
  return result;
}

mojom::DeviceStatePropertiesPtr DeviceStateToMojo(
    const DeviceState* device,
    mojom::DeviceStateType technology_state) {
  mojom::NetworkType type = ShillTypeToMojo(device->type());
  if (type == mojom::NetworkType::kAll) {
    NET_LOG(ERROR) << "Unexpected device type: " << device->type()
                   << " path: " << device->path();
    return nullptr;
  }

  auto result = mojom::DeviceStateProperties::New();
  result->type = type;
  net::IPAddress ipv4_address;
  if (ipv4_address.AssignFromIPLiteral(
          device->GetIpAddressByType(shill::kTypeIPv4))) {
    result->ipv4_address = ipv4_address;
  }
  net::IPAddress ipv6_address;
  if (ipv6_address.AssignFromIPLiteral(
          device->GetIpAddressByType(shill::kTypeIPv6))) {
    result->ipv6_address = ipv6_address;
  }
  result->mac_address =
      network_util::FormattedMacAddress(device->mac_address());
  result->scanning = device->scanning();
  result->device_state = technology_state;
  result->managed_network_available =
      !device->available_managed_network_path().empty();
  result->sim_absent = device->IsSimAbsent();
  if (device->sim_present()) {
    auto sim_lock_status = mojom::SIMLockStatus::New();
    sim_lock_status->lock_type = device->sim_lock_type();
    sim_lock_status->lock_enabled = device->sim_lock_enabled();
    sim_lock_status->retries_left = device->sim_retries_left();
    result->sim_lock_status = std::move(sim_lock_status);
  }
  return result;
}

void SetValueIfKeyPresent(const base::Value* dict,
                          const char* key,
                          base::Value* out) {
  const base::Value* v = dict->FindKey(key);
  if (v)
    *out = v->Clone();
}

base::Optional<std::string> GetString(const base::Value* dict,
                                      const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_string()) {
    NET_LOG(ERROR) << "Expected string, found: " << *v;
    return base::nullopt;
  }
  return v ? base::make_optional<std::string>(v->GetString()) : base::nullopt;
}

std::string GetRequiredString(const base::Value* dict, const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v) {
    NOTREACHED() << "Required key missing: " << key;
    return std::string();
  }
  if (!v->is_string()) {
    NET_LOG(ERROR) << "Expected string, found: " << *v;
    return std::string();
  }
  return v->GetString();
}

bool GetBoolean(const base::Value* dict, const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_bool()) {
    NET_LOG(ERROR) << "Expected bool, found: " << *v;
    return false;
  }
  return v ? v->GetBool() : false;
}

int32_t GetInt32(const base::Value* dict, const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_int()) {
    NET_LOG(ERROR) << "Expected int, found: " << *v;
    return false;
  }
  return v ? v->GetInt() : false;
}

std::vector<int32_t> GetInt32List(const base::Value* dict, const char* key) {
  std::vector<int32_t> result;
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_list()) {
    NET_LOG(ERROR) << "Expected list, found: " << *v;
    return result;
  }
  if (v) {
    for (const base::Value& e : v->GetList())
      result.push_back(e.GetInt());
  }
  return result;
}

const base::Value* GetDictionary(const base::Value* dict, const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *v;
    return nullptr;
  }
  return v;
}

base::Optional<std::vector<std::string>> GetStringList(const base::Value* dict,
                                                       const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return base::nullopt;
  if (!v->is_list()) {
    NET_LOG(ERROR) << "Expected list, found: " << *v;
    return base::nullopt;
  }
  std::vector<std::string> result;
  for (const base::Value& e : v->GetList())
    result.push_back(e.GetString());
  return result;
}

void SetString(const char* key,
               const base::Optional<std::string>& property,
               base::Value* dict) {
  if (!property)
    return;
  dict->SetStringKey(key, *property);
}

void SetStringIfNotEmpty(const char* key,
                         const base::Optional<std::string>& property,
                         base::Value* dict) {
  if (!property || property->empty())
    return;
  dict->SetStringKey(key, *property);
}

void SetStringList(const char* key,
                   const base::Optional<std::vector<std::string>>& property,
                   base::Value* dict) {
  if (!property)
    return;
  base::Value list(base::Value::Type::LIST);
  for (const std::string& s : *property)
    list.Append(base::Value(s));
  dict->SetKey(key, std::move(list));
}

// GetManagedDictionary() returns a ManagedDictionary representing the active
// and policy values for a managed property. The types of |active_value| and
// |policy_value| are expected to match the ONC signature for the property type.

struct ManagedDictionary {
  base::Value active_value;
  mojom::PolicySource policy_source = mojom::PolicySource::kNone;
  base::Value policy_value;
};

ManagedDictionary GetManagedDictionary(const base::Value* onc_dict) {
  ManagedDictionary result;

  // When available, the active value (i.e. the value from Shill) is used.
  SetValueIfKeyPresent(onc_dict, ::onc::kAugmentationActiveSetting,
                       &result.active_value);

  base::Optional<std::string> effective =
      GetString(onc_dict, ::onc::kAugmentationEffectiveSetting);
  if (!effective)
    return result;

  // If no active value is set (e.g. the network is not visible), use the
  // effective value.
  if (result.active_value.is_none())
    SetValueIfKeyPresent(onc_dict, effective->c_str(), &result.active_value);
  if (result.active_value.is_none()) {
    // No active or effective value, return a default dictionary.
    return result;
  }

  // If the effective value is set by an extension, use kActiveExtension.
  if (effective == ::onc::kAugmentationActiveExtension) {
    result.policy_source = mojom::PolicySource::kActiveExtension;
    result.policy_value = result.active_value.Clone();
    return result;
  }

  // Set policy properties based on the effective source and policies.
  // NOTE: This does not enforce valid ONC. See onc_merger.cc for details.
  const base::Value* user_policy =
      onc_dict->FindKey(::onc::kAugmentationUserPolicy);
  const base::Value* device_policy =
      onc_dict->FindKey(::onc::kAugmentationDevicePolicy);
  bool user_enforced = !GetBoolean(onc_dict, ::onc::kAugmentationUserEditable);
  bool device_enforced =
      !GetBoolean(onc_dict, ::onc::kAugmentationDeviceEditable);
  if (effective == ::onc::kAugmentationUserPolicy ||
      (user_policy && effective != ::onc::kAugmentationDevicePolicy)) {
    // Set the policy source to "User" when:
    // * The effective value is set to "UserPolicy" OR
    // * A User policy exists and the effective value is not "DevicePolicy",
    //   i.e. no enforced device policy is overriding a recommended user policy.
    result.policy_source = user_enforced
                               ? mojom::PolicySource::kUserPolicyEnforced
                               : mojom::PolicySource::kUserPolicyRecommended;
    if (user_policy)
      result.policy_value = user_policy->Clone();
  } else if (effective == ::onc::kAugmentationDevicePolicy || device_policy) {
    // Set the policy source to "Device" when:
    // * The effective value is set to "DevicePolicy" OR
    // * A Device policy exists (since we checked for a user policy first).
    result.policy_source = device_enforced
                               ? mojom::PolicySource::kDevicePolicyEnforced
                               : mojom::PolicySource::kDevicePolicyRecommended;
    if (device_policy)
      result.policy_value = device_policy->Clone();
  } else if (effective == ::onc::kAugmentationUserSetting ||
             effective == ::onc::kAugmentationSharedSetting) {
    // User or shared setting, no policy source.
  } else {
    // Unexpected ONC. No policy source or value will be set.
    NET_LOG(ERROR) << "Unexpected ONC property: " << *onc_dict;
  }

  DCHECK(result.policy_value.is_none() ||
         result.policy_value.type() == result.active_value.type());
  return result;
}

mojom::ManagedStringPtr GetManagedString(const base::Value* dict,
                                         const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return nullptr;
  if (v->is_string()) {
    auto result = mojom::ManagedString::New();
    result->active_value = v->GetString();
    return result;
  }
  if (v->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(v);
    if (!managed_dict.active_value.is_string()) {
      NET_LOG(ERROR) << "No active or effective value for: " << key;
      return nullptr;
    }
    auto result = mojom::ManagedString::New();
    result->active_value = managed_dict.active_value.GetString();
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none())
      result->policy_value = managed_dict.policy_value.GetString();
    return result;
  }
  NET_LOG(ERROR) << "Expected string or dictionary, found: " << *v;
  return nullptr;
}

mojom::ManagedStringPtr GetRequiredManagedString(const base::Value* dict,
                                                 const char* key) {
  mojom::ManagedStringPtr result = GetManagedString(dict, key);
  if (!result) {
    // Return an empty string with no policy source.
    result = mojom::ManagedString::New();
  }
  return result;
}

mojom::ManagedStringListPtr GetManagedStringList(const base::Value* dict,
                                                 const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return nullptr;
  if (v->is_list()) {
    auto result = mojom::ManagedStringList::New();
    std::vector<std::string> active;
    for (const base::Value& e : v->GetList())
      active.push_back(e.GetString());
    result->active_value = std::move(active);
    return result;
  }
  if (v->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(v);
    if (!managed_dict.active_value.is_list()) {
      NET_LOG(ERROR) << "No active or effective value for: " << key;
      return nullptr;
    }
    auto result = mojom::ManagedStringList::New();
    for (const base::Value& e : managed_dict.active_value.GetList())
      result->active_value.push_back(e.GetString());
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none()) {
      result->policy_value = std::vector<std::string>();
      for (const base::Value& e : managed_dict.policy_value.GetList())
        result->policy_value->push_back(e.GetString());
    }
    return result;
  }
  NET_LOG(ERROR) << "Expected list or dictionary, found: " << *v;
  return nullptr;
}

mojom::ManagedBooleanPtr GetManagedBoolean(const base::Value* dict,
                                           const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return nullptr;
  if (v->is_bool()) {
    auto result = mojom::ManagedBoolean::New();
    result->active_value = v->GetBool();
    return result;
  }
  if (v->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(v);
    if (!managed_dict.active_value.is_bool()) {
      NET_LOG(ERROR) << "No active or effective value for: " << key;
      return nullptr;
    }
    auto result = mojom::ManagedBoolean::New();
    result->active_value = managed_dict.active_value.GetBool();
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none())
      result->policy_value = managed_dict.policy_value.GetBool();
    return result;
  }
  NET_LOG(ERROR) << "Expected bool or dictionary, found: " << *v;
  return nullptr;
}

mojom::ManagedInt32Ptr GetManagedInt32(const base::Value* dict,
                                       const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return nullptr;
  if (v->is_int()) {
    auto result = mojom::ManagedInt32::New();
    result->active_value = v->GetInt();
    return result;
  }
  if (v->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(v);
    if (!managed_dict.active_value.is_int()) {
      NET_LOG(ERROR) << "No active or effective value for: " << key;
      return nullptr;
    }
    auto result = mojom::ManagedInt32::New();
    result->active_value = managed_dict.active_value.GetInt();
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none())
      result->policy_value = managed_dict.policy_value.GetInt();
    return result;
  }
  NET_LOG(ERROR) << "Expected int or dictionary, found: " << *v;
  return nullptr;
}

mojom::IPConfigPropertiesPtr GetIPConfig(const base::Value* dict) {
  auto ip_config = mojom::IPConfigProperties::New();
  ip_config->gateway = GetString(dict, ::onc::ipconfig::kGateway);
  ip_config->ip_address = GetString(dict, ::onc::ipconfig::kIPAddress);
  ip_config->excluded_routes =
      GetStringList(dict, ::onc::ipconfig::kExcludedRoutes);
  ip_config->included_routes =
      GetStringList(dict, ::onc::ipconfig::kIncludedRoutes);
  ip_config->name_servers = GetStringList(dict, ::onc::ipconfig::kNameServers);
  ip_config->search_domains =
      GetStringList(dict, ::onc::ipconfig::kSearchDomains);
  ip_config->routing_prefix = GetInt32(dict, ::onc::ipconfig::kRoutingPrefix);
  ip_config->type = GetString(dict, ::onc::ipconfig::kType);
  // Shill may omit the IP Config type for VPNs. The type should be IPv4.
  if (!ip_config->type || ip_config->type->empty())
    ip_config->type = ::onc::ipconfig::kIPv4;
  ip_config->web_proxy_auto_discovery_url =
      GetString(dict, ::onc::ipconfig::kWebProxyAutoDiscoveryUrl);
  return ip_config;
}

mojom::ManagedIPConfigPropertiesPtr GetManagedIPConfig(
    const base::Value* dict) {
  auto ip_config = mojom::ManagedIPConfigProperties::New();
  ip_config->gateway = GetManagedString(dict, ::onc::ipconfig::kGateway);
  ip_config->ip_address = GetManagedString(dict, ::onc::ipconfig::kIPAddress);
  ip_config->name_servers =
      GetManagedStringList(dict, ::onc::ipconfig::kNameServers);
  ip_config->routing_prefix =
      GetManagedInt32(dict, ::onc::ipconfig::kRoutingPrefix);
  ip_config->type = GetManagedString(dict, ::onc::ipconfig::kType);
  ip_config->web_proxy_auto_discovery_url =
      GetManagedString(dict, ::onc::ipconfig::kWebProxyAutoDiscoveryUrl);
  return ip_config;
}

mojom::ManagedProxyLocationPtr GetManagedProxyLocation(const base::Value* dict,
                                                       const char* key) {
  const base::Value* location_dict = GetDictionary(dict, key);
  if (!location_dict)
    return nullptr;
  auto proxy_location = mojom::ManagedProxyLocation::New();
  proxy_location->host =
      GetRequiredManagedString(location_dict, ::onc::proxy::kHost);
  proxy_location->port = GetManagedInt32(location_dict, ::onc::proxy::kPort);
  if (!proxy_location->port) {
    NET_LOG(ERROR) << "ProxyLocation: No port: " << *location_dict;
    return nullptr;
  }
  return proxy_location;
}

void SetProxyLocation(const char* key,
                      const mojom::ProxyLocationPtr& location,
                      base::Value* dict) {
  if (location.is_null())
    return;
  base::Value location_dict(base::Value::Type::DICTIONARY);
  location_dict.SetStringKey(::onc::proxy::kHost, location->host);
  location_dict.SetIntKey(::onc::proxy::kPort, location->port);
  dict->SetKey(key, std::move(location_dict));
}

mojom::ManagedProxySettingsPtr GetManagedProxySettings(
    const base::Value* dict) {
  auto proxy_settings = mojom::ManagedProxySettings::New();
  proxy_settings->type = GetRequiredManagedString(dict, ::onc::proxy::kType);
  const base::Value* manual_dict = GetDictionary(dict, ::onc::proxy::kManual);
  if (manual_dict) {
    auto manual_proxy_settings = mojom::ManagedManualProxySettings::New();
    manual_proxy_settings->http_proxy =
        GetManagedProxyLocation(manual_dict, ::onc::proxy::kHttp);
    manual_proxy_settings->secure_http_proxy =
        GetManagedProxyLocation(manual_dict, ::onc::proxy::kHttps);
    manual_proxy_settings->ftp_proxy =
        GetManagedProxyLocation(manual_dict, ::onc::proxy::kFtp);
    manual_proxy_settings->socks =
        GetManagedProxyLocation(manual_dict, ::onc::proxy::kSocks);
    proxy_settings->manual = std::move(manual_proxy_settings);
  }
  proxy_settings->exclude_domains =
      GetManagedStringList(dict, ::onc::proxy::kExcludeDomains);
  proxy_settings->pac = GetManagedString(dict, ::onc::proxy::kPAC);
  return proxy_settings;
}

mojom::ApnPropertiesPtr GetApnProperties(const base::Value* dict) {
  auto apn = mojom::ApnProperties::New();
  apn->access_point_name =
      GetRequiredString(dict, ::onc::cellular_apn::kAccessPointName);
  apn->authentication = GetString(dict, ::onc::cellular_apn::kAuthentication);
  apn->language = GetString(dict, ::onc::cellular_apn::kLanguage);
  apn->localized_name = GetString(dict, ::onc::cellular_apn::kLocalizedName);
  apn->name = GetString(dict, ::onc::cellular_apn::kName);
  apn->password = GetString(dict, ::onc::cellular_apn::kPassword);
  apn->username = GetString(dict, ::onc::cellular_apn::kUsername);
  return apn;
}

mojom::ManagedApnPropertiesPtr GetManagedApnProperties(const base::Value* dict,
                                                       const char* key) {
  const base::Value* apn_dict = dict->FindKey(key);
  if (!apn_dict)
    return nullptr;
  if (!apn_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *apn_dict;
    return nullptr;
  }
  auto apn = mojom::ManagedApnProperties::New();
  apn->access_point_name =
      GetRequiredManagedString(apn_dict, ::onc::cellular_apn::kAccessPointName);
  CHECK(apn->access_point_name);
  apn->authentication =
      GetManagedString(apn_dict, ::onc::cellular_apn::kAuthentication);
  apn->language = GetManagedString(apn_dict, ::onc::cellular_apn::kLanguage);
  apn->localized_name =
      GetManagedString(apn_dict, ::onc::cellular_apn::kLocalizedName);
  apn->name = GetManagedString(apn_dict, ::onc::cellular_apn::kName);
  apn->password = GetManagedString(apn_dict, ::onc::cellular_apn::kPassword);
  apn->username = GetManagedString(apn_dict, ::onc::cellular_apn::kUsername);
  return apn;
}

mojom::ManagedApnListPtr GetManagedApnList(const base::Value* value) {
  if (!value)
    return nullptr;
  if (value->is_list()) {
    auto result = mojom::ManagedApnList::New();
    std::vector<mojom::ApnPropertiesPtr> active;
    for (const base::Value& value : value->GetList())
      active.push_back(GetApnProperties(&value));
    result->active_value = std::move(active);
    return result;
  } else if (value->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(value);
    if (!managed_dict.active_value.is_list()) {
      NET_LOG(ERROR) << "No active or effective value for APNList";
      return nullptr;
    }
    auto result = mojom::ManagedApnList::New();
    for (const base::Value& e : managed_dict.active_value.GetList())
      result->active_value.push_back(GetApnProperties(&e));
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none()) {
      result->policy_value = std::vector<mojom::ApnPropertiesPtr>();
      for (const base::Value& e : managed_dict.policy_value.GetList())
        result->policy_value->push_back(GetApnProperties(&e));
    }
    return result;
  }
  NET_LOG(ERROR) << "Expected list or dictionary, found: " << *value;
  return nullptr;
}

std::vector<mojom::FoundNetworkPropertiesPtr> GetFoundNetworksList(
    const base::Value* dict,
    const char* key) {
  std::vector<mojom::FoundNetworkPropertiesPtr> result;
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return result;
  if (!v->is_list()) {
    NET_LOG(ERROR) << "Expected list, found: " << *v;
    return result;
  }
  for (const base::Value& e : v->GetList()) {
    auto found_network = mojom::FoundNetworkProperties::New();
    found_network->status =
        GetRequiredString(&e, ::onc::cellular_found_network::kStatus);
    found_network->network_id =
        GetRequiredString(&e, ::onc::cellular_found_network::kNetworkId);
    found_network->technology =
        GetRequiredString(&e, ::onc::cellular_found_network::kTechnology);
    found_network->short_name =
        GetString(&e, ::onc::cellular_found_network::kShortName);
    found_network->long_name =
        GetString(&e, ::onc::cellular_found_network::kLongName);
    result.push_back(std::move(found_network));
  }
  return result;
}

mojom::CellularProviderPropertiesPtr GetCellularProviderProperties(
    const base::Value* dict,
    const char* key) {
  const base::Value* provider_dict = dict->FindKey(key);
  if (!provider_dict)
    return nullptr;
  auto provider = mojom::CellularProviderProperties::New();
  provider->name =
      GetRequiredString(provider_dict, ::onc::cellular_provider::kName);
  provider->code =
      GetRequiredString(provider_dict, ::onc::cellular_provider::kCode);
  provider->country =
      GetString(provider_dict, ::onc::cellular_provider::kCountry);
  return provider;
}

mojom::ManagedIssuerSubjectPatternPtr GetManagedIssuerSubjectPattern(
    const base::Value* dict,
    const char* key) {
  const base::Value* pattern_dict = dict->FindKey(key);
  if (!pattern_dict)
    return nullptr;
  if (!pattern_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *pattern_dict;
    return nullptr;
  }
  auto pattern = mojom::ManagedIssuerSubjectPattern::New();
  pattern->common_name =
      GetManagedString(pattern_dict, ::onc::client_cert::kCommonName);
  pattern->locality =
      GetManagedString(pattern_dict, ::onc::client_cert::kLocality);
  pattern->organization =
      GetManagedString(pattern_dict, ::onc::client_cert::kOrganization);
  pattern->organizational_unit =
      GetManagedString(pattern_dict, ::onc::client_cert::kOrganizationalUnit);
  return pattern;
}

mojom::ManagedCertificatePatternPtr GetManagedCertificatePattern(
    const base::Value* dict,
    const char* key) {
  const base::Value* pattern_dict = dict->FindKey(key);
  if (!pattern_dict)
    return nullptr;
  if (!pattern_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *pattern_dict;
    return nullptr;
  }
  auto pattern = mojom::ManagedCertificatePattern::New();
  pattern->enrollment_uri =
      GetManagedStringList(pattern_dict, ::onc::client_cert::kEnrollmentURI);
  pattern->issuer =
      GetManagedIssuerSubjectPattern(pattern_dict, ::onc::client_cert::kIssuer);
  pattern->issuer_ca_ref =
      GetManagedStringList(pattern_dict, ::onc::client_cert::kIssuerCARef);
  pattern->subject = GetManagedIssuerSubjectPattern(
      pattern_dict, ::onc::client_cert::kSubject);
  return pattern;
}

mojom::ManagedEAPPropertiesPtr GetManagedEAPProperties(const base::Value* dict,
                                                       const char* key) {
  auto eap = mojom::ManagedEAPProperties::New();
  const base::Value* eap_dict = dict->FindKey(key);
  if (!eap_dict)
    return eap;
  if (!eap_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *eap_dict;
    return eap;
  }
  eap->anonymous_identity =
      GetManagedString(eap_dict, ::onc::eap::kAnonymousIdentity);
  eap->client_cert_pattern = GetManagedCertificatePattern(
      eap_dict, ::onc::client_cert::kClientCertPattern);
  eap->client_cert_pkcs11_id =
      GetManagedString(eap_dict, ::onc::client_cert::kClientCertPKCS11Id);
  eap->client_cert_ref =
      GetManagedString(eap_dict, ::onc::client_cert::kClientCertRef);
  eap->client_cert_type =
      GetManagedString(eap_dict, ::onc::client_cert::kClientCertType);
  eap->identity = GetManagedString(eap_dict, ::onc::eap::kIdentity);
  eap->inner = GetManagedString(eap_dict, ::onc::eap::kInner);
  eap->outer = GetManagedString(eap_dict, ::onc::eap::kOuter);
  eap->password = GetManagedString(eap_dict, ::onc::eap::kPassword);
  eap->save_credentials =
      GetManagedBoolean(eap_dict, ::onc::eap::kSaveCredentials);
  eap->server_ca_pems =
      GetManagedStringList(eap_dict, ::onc::eap::kServerCAPEMs);
  eap->server_ca_refs =
      GetManagedStringList(eap_dict, ::onc::eap::kServerCARefs);
  eap->subject_match = GetManagedString(eap_dict, ::onc::eap::kSubjectMatch);
  eap->tls_version_max = GetManagedString(eap_dict, ::onc::eap::kTLSVersionMax);
  eap->use_proactive_key_caching =
      GetManagedBoolean(eap_dict, ::onc::eap::kUseProactiveKeyCaching);
  eap->use_system_cas = GetManagedBoolean(eap_dict, ::onc::eap::kUseSystemCAs);
  return eap;
}

mojom::ManagedIPSecPropertiesPtr GetManagedIPSecProperties(
    const base::Value* dict,
    const char* key) {
  auto ipsec = mojom::ManagedIPSecProperties::New();
  const base::Value* ipsec_dict = dict->FindKey(key);
  if (!ipsec_dict)
    return ipsec;
  if (!ipsec_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *ipsec_dict;
    return ipsec;
  }
  ipsec->authentication_type =
      GetRequiredManagedString(ipsec_dict, ::onc::ipsec::kAuthenticationType);
  ipsec->client_cert_pattern = GetManagedCertificatePattern(
      ipsec_dict, ::onc::client_cert::kClientCertPattern);
  ipsec->client_cert_pkcs11_id =
      GetManagedString(ipsec_dict, ::onc::client_cert::kClientCertPKCS11Id);
  ipsec->client_cert_ref =
      GetManagedString(ipsec_dict, ::onc::client_cert::kClientCertRef);
  ipsec->client_cert_type =
      GetManagedString(ipsec_dict, ::onc::client_cert::kClientCertType);
  ipsec->eap = GetManagedEAPProperties(ipsec_dict, ::onc::ipsec::kEAP);
  ipsec->group = GetManagedString(ipsec_dict, ::onc::ipsec::kGroup);
  ipsec->ike_version = GetManagedInt32(ipsec_dict, ::onc::ipsec::kIKEVersion);
  ipsec->psk = GetManagedString(ipsec_dict, ::onc::ipsec::kPSK);
  ipsec->save_credentials =
      GetManagedBoolean(ipsec_dict, ::onc::vpn::kSaveCredentials);
  ipsec->server_ca_pems =
      GetManagedStringList(ipsec_dict, ::onc::ipsec::kServerCAPEMs);
  ipsec->server_ca_refs =
      GetManagedStringList(ipsec_dict, ::onc::ipsec::kServerCARefs);
  return ipsec;
}

mojom::ManagedL2TPPropertiesPtr GetManagedL2TPProperties(
    const base::Value* dict,
    const char* key) {
  auto l2tp = mojom::ManagedL2TPProperties::New();
  const base::Value* l2tp_dict = dict->FindKey(key);
  if (!l2tp_dict)
    return l2tp;
  if (!l2tp_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *l2tp_dict;
    return l2tp;
  }
  l2tp->lcp_echo_disabled =
      GetManagedBoolean(l2tp_dict, ::onc::l2tp::kLcpEchoDisabled);
  l2tp->password = GetManagedString(l2tp_dict, ::onc::l2tp::kPassword);
  l2tp->save_credentials =
      GetManagedBoolean(l2tp_dict, ::onc::l2tp::kSaveCredentials);
  l2tp->username = GetManagedString(l2tp_dict, ::onc::l2tp::kUsername);
  return l2tp;
}

mojom::ManagedOpenVPNPropertiesPtr GetManagedOpenVPNProperties(
    const base::Value* dict,
    const char* key) {
  auto openvpn = mojom::ManagedOpenVPNProperties::New();
  const base::Value* openvpn_dict = dict->FindKey(key);
  if (!openvpn_dict)
    return openvpn;
  if (!openvpn_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *openvpn_dict;
    return openvpn;
  }
  openvpn->auth = GetManagedString(openvpn_dict, ::onc::openvpn::kAuth);
  openvpn->auth_retry =
      GetManagedString(openvpn_dict, ::onc::openvpn::kAuthRetry);
  openvpn->auth_no_cache =
      GetManagedBoolean(openvpn_dict, ::onc::openvpn::kAuthNoCache);
  openvpn->cipher = GetManagedString(openvpn_dict, ::onc::openvpn::kCipher);
  openvpn->client_cert_pkcs11_id =
      GetManagedString(openvpn_dict, ::onc::client_cert::kClientCertPKCS11Id);
  openvpn->client_cert_pattern = GetManagedCertificatePattern(
      openvpn_dict, ::onc::client_cert::kClientCertPattern);
  openvpn->client_cert_ref =
      GetManagedString(openvpn_dict, ::onc::client_cert::kClientCertRef);
  openvpn->client_cert_type =
      GetManagedString(openvpn_dict, ::onc::client_cert::kClientCertType);
  openvpn->comp_lzo = GetManagedString(openvpn_dict, ::onc::openvpn::kCompLZO);
  openvpn->comp_no_adapt =
      GetManagedBoolean(openvpn_dict, ::onc::openvpn::kCompNoAdapt);
  openvpn->extra_hosts =
      GetManagedStringList(openvpn_dict, ::onc::openvpn::kExtraHosts);
  openvpn->ignore_default_route =
      GetManagedBoolean(openvpn_dict, ::onc::openvpn::kIgnoreDefaultRoute);
  openvpn->key_direction =
      GetManagedString(openvpn_dict, ::onc::openvpn::kKeyDirection);
  openvpn->ns_cert_type =
      GetManagedString(openvpn_dict, ::onc::openvpn::kNsCertType);
  openvpn->password = GetManagedString(openvpn_dict, ::onc::openvpn::kPassword);
  openvpn->port = GetManagedInt32(openvpn_dict, ::onc::openvpn::kPort);
  openvpn->proto = GetManagedString(openvpn_dict, ::onc::openvpn::kProto);
  openvpn->push_peer_info =
      GetManagedBoolean(openvpn_dict, ::onc::openvpn::kPushPeerInfo);
  openvpn->remote_cert_eku =
      GetManagedString(openvpn_dict, ::onc::openvpn::kRemoteCertEKU);
  openvpn->remote_cert_ku =
      GetManagedStringList(openvpn_dict, ::onc::openvpn::kRemoteCertKU);
  openvpn->remote_cert_tls =
      GetManagedString(openvpn_dict, ::onc::openvpn::kRemoteCertTLS);
  openvpn->reneg_sec = GetManagedInt32(openvpn_dict, ::onc::openvpn::kRenegSec);
  openvpn->save_credentials =
      GetManagedBoolean(openvpn_dict, ::onc::vpn::kSaveCredentials);
  openvpn->server_ca_pems =
      GetManagedStringList(openvpn_dict, ::onc::openvpn::kServerCAPEMs);
  openvpn->server_ca_refs =
      GetManagedStringList(openvpn_dict, ::onc::openvpn::kServerCARefs);
  openvpn->server_cert_ref =
      GetManagedString(openvpn_dict, ::onc::openvpn::kServerCertRef);
  openvpn->server_poll_timeout =
      GetManagedInt32(openvpn_dict, ::onc::openvpn::kServerPollTimeout);
  openvpn->shaper = GetManagedInt32(openvpn_dict, ::onc::openvpn::kShaper);
  openvpn->static_challenge =
      GetManagedString(openvpn_dict, ::onc::openvpn::kStaticChallenge);
  openvpn->tls_auth_contents =
      GetManagedString(openvpn_dict, ::onc::openvpn::kTLSAuthContents);
  openvpn->tls_remote =
      GetManagedString(openvpn_dict, ::onc::openvpn::kTLSRemote);
  openvpn->tls_version_min =
      GetManagedString(openvpn_dict, ::onc::openvpn::kTLSVersionMin);
  openvpn->user_authentication_type =
      GetManagedString(openvpn_dict, ::onc::openvpn::kUserAuthenticationType);
  openvpn->username = GetManagedString(openvpn_dict, ::onc::vpn::kUsername);
  openvpn->verb = GetManagedString(openvpn_dict, ::onc::openvpn::kVerb);
  openvpn->verify_hash =
      GetManagedString(openvpn_dict, ::onc::openvpn::kVerifyHash);
  const base::Value* verify_x509_dict =
      openvpn_dict->FindKey(::onc::openvpn::kVerifyX509);
  if (verify_x509_dict) {
    auto verify_x509 = mojom::ManagedVerifyX509Properties::New();
    verify_x509->name =
        GetManagedString(verify_x509_dict, ::onc::verify_x509::kName);
    verify_x509->type =
        GetManagedString(verify_x509_dict, ::onc::verify_x509::kType);
    openvpn->verify_x509 = std::move(verify_x509);
  }
  return openvpn;
}

mojom::ManagedPropertiesPtr ManagedPropertiesToMojo(
    const NetworkState* network_state,
    const std::vector<mojom::VpnProviderPtr>& vpn_providers,
    const base::DictionaryValue* properties) {
  DCHECK(network_state);
  DCHECK(properties);
  base::Optional<std::string> onc_type =
      GetString(properties, ::onc::network_config::kType);
  if (!onc_type) {
    NET_LOG(ERROR) << "Malformed ONC dictionary: missing 'Type'";
    return nullptr;
  }
  mojom::NetworkType type = OncTypeToMojo(*onc_type);
  if (type == mojom::NetworkType::kAll) {
    NET_LOG(ERROR) << "Unexpected network type: " << *onc_type;
    return nullptr;
  }
  auto result = mojom::ManagedProperties::New();

  // |network_state| and |properties| guid should be the same.
  base::Optional<std::string> guid =
      GetString(properties, ::onc::network_config::kGUID);
  if (!guid) {
    NET_LOG(ERROR) << "Malformed ONC dictionary: missing 'GUID'";
    return nullptr;
  }
  DCHECK_EQ(network_state->guid(), *guid);
  result->guid = network_state->guid();

  // Typed properties (compatible with NetworkStateProperties):
  result->connection_state =
      GetConnectionState(network_state, /*technology_enabled=*/true);
  result->source = GetMojoOncSource(network_state);
  result->type = type;

  // Unmanaged properties
  result->connectable =
      GetBoolean(properties, ::onc::network_config::kConnectable);
  result->error_state =
      GetString(properties, ::onc::network_config::kErrorState);
  const base::Value* ip_configs_list =
      properties->FindKey(::onc::network_config::kIPConfigs);
  if (ip_configs_list) {
    std::vector<mojom::IPConfigPropertiesPtr> ip_configs;
    for (const base::Value& ip_config_value : ip_configs_list->GetList())
      ip_configs.push_back(GetIPConfig(&ip_config_value));
    result->ip_configs = std::move(ip_configs);
  }
  result->restricted_connectivity =
      GetBoolean(properties, ::onc::network_config::kRestrictedConnectivity);
  const base::Value* saved_ip_config =
      GetDictionary(properties, ::onc::network_config::kSavedIPConfig);
  if (saved_ip_config)
    result->saved_ip_config = GetIPConfig(saved_ip_config);

  // Managed properties
  result->ip_address_config_type =
      GetManagedString(properties, ::onc::network_config::kIPAddressConfigType);
  result->name = GetManagedString(properties, ::onc::network_config::kName);
  result->name_servers_config_type = GetManagedString(
      properties, ::onc::network_config::kNameServersConfigType);
  result->priority =
      GetManagedInt32(properties, ::onc::network_config::kPriority);

  // Managed dictionaries (not type specific)
  const base::Value* proxy_settings =
      GetDictionary(properties, ::onc::network_config::kProxySettings);
  if (proxy_settings)
    result->proxy_settings = GetManagedProxySettings(proxy_settings);
  const base::Value* static_ip_config =
      GetDictionary(properties, ::onc::network_config::kStaticIPConfig);
  if (static_ip_config)
    result->static_ip_config = GetManagedIPConfig(static_ip_config);

  // Type specific dictionaries
  switch (type) {
    case mojom::NetworkType::kCellular: {
      auto cellular = mojom::ManagedCellularProperties::New();
      cellular->activation_state = network_state->GetMojoActivationState();

      const base::Value* cellular_dict =
          GetDictionary(properties, ::onc::network_config::kCellular);
      if (!cellular_dict) {
        result->type_properties =
            mojom::NetworkTypeManagedProperties::NewCellular(
                std::move(cellular));
        break;
      }
      cellular->auto_connect =
          GetManagedBoolean(cellular_dict, ::onc::cellular::kAutoConnect);
      cellular->apn =
          GetManagedApnProperties(cellular_dict, ::onc::cellular::kAPN);
      cellular->apn_list =
          GetManagedApnList(cellular_dict->FindKey(::onc::cellular::kAPNList));
      cellular->allow_roaming =
          GetBoolean(cellular_dict, ::onc::cellular::kAllowRoaming);
      cellular->esn = GetString(cellular_dict, ::onc::cellular::kESN);
      cellular->family = GetString(cellular_dict, ::onc::cellular::kFamily);
      cellular->firmware_revision =
          GetString(cellular_dict, ::onc::cellular::kFirmwareRevision);
      cellular->found_networks =
          GetFoundNetworksList(cellular_dict, ::onc::cellular::kFoundNetworks);
      cellular->hardware_revision =
          GetString(cellular_dict, ::onc::cellular::kHardwareRevision);
      cellular->home_provider = GetCellularProviderProperties(
          cellular_dict, ::onc::cellular::kHomeProvider);
      cellular->iccid = GetString(cellular_dict, ::onc::cellular::kICCID);
      cellular->imei = GetString(cellular_dict, ::onc::cellular::kIMEI);
      const base::Value* apn_dict =
          GetDictionary(cellular_dict, ::onc::cellular::kLastGoodAPN);
      if (apn_dict)
        cellular->last_good_apn = GetApnProperties(apn_dict);
      cellular->manufacturer =
          GetString(cellular_dict, ::onc::cellular::kManufacturer);
      cellular->mdn = GetString(cellular_dict, ::onc::cellular::kMDN);
      cellular->meid = GetString(cellular_dict, ::onc::cellular::kMEID);
      cellular->min = GetString(cellular_dict, ::onc::cellular::kMIN);
      cellular->model_id = GetString(cellular_dict, ::onc::cellular::kModelID);
      cellular->network_technology =
          GetString(cellular_dict, ::onc::cellular::kNetworkTechnology);
      const base::Value* payment_portal_dict =
          cellular_dict->FindKey(::onc::cellular::kPaymentPortal);
      if (payment_portal_dict) {
        auto payment_portal = mojom::PaymentPortalProperties::New();
        payment_portal->method = GetRequiredString(
            payment_portal_dict, ::onc::cellular_payment_portal::kMethod);
        payment_portal->post_data = GetRequiredString(
            payment_portal_dict, ::onc::cellular_payment_portal::kPostData);
        payment_portal->url = GetString(payment_portal_dict,
                                        ::onc::cellular_payment_portal::kUrl);
        cellular->payment_portal = std::move(payment_portal);
      }
      cellular->roaming_state =
          GetString(cellular_dict, ::onc::cellular::kRoamingState);
      cellular->serving_operator = GetCellularProviderProperties(
          cellular_dict, ::onc::cellular::kServingOperator);
      cellular->signal_strength =
          GetInt32(cellular_dict, ::onc::cellular::kSignalStrength);
      cellular->support_network_scan =
          GetBoolean(cellular_dict, ::onc::cellular::kSupportNetworkScan);
      result->type_properties =
          mojom::NetworkTypeManagedProperties::NewCellular(std::move(cellular));
      break;
    }
    case mojom::NetworkType::kEthernet: {
      auto ethernet = mojom::ManagedEthernetProperties::New();
      const base::Value* ethernet_dict =
          GetDictionary(properties, ::onc::network_config::kEthernet);
      if (ethernet_dict) {
        ethernet->authentication =
            GetManagedString(ethernet_dict, ::onc::ethernet::kAuthentication);
        ethernet->eap =
            GetManagedEAPProperties(ethernet_dict, ::onc::ethernet::kEAP);
      }
      result->type_properties =
          mojom::NetworkTypeManagedProperties::NewEthernet(std::move(ethernet));
      break;
    }
    case mojom::NetworkType::kTether: {
      // Tether has no managed properties, provide the state properties.
      auto tether = mojom::TetherStateProperties::New();
      tether->battery_percentage = network_state->battery_percentage();
      tether->carrier = network_state->tether_carrier();
      tether->has_connected_to_host =
          network_state->tether_has_connected_to_host();
      tether->signal_strength = network_state->signal_strength();
      result->type_properties =
          mojom::NetworkTypeManagedProperties::NewTether(std::move(tether));
      break;
    }
    case mojom::NetworkType::kVPN: {
      auto vpn = mojom::ManagedVPNProperties::New();
      const base::Value* vpn_dict =
          GetDictionary(properties, ::onc::network_config::kVPN);
      if (!vpn_dict) {
        result->type_properties =
            mojom::NetworkTypeManagedProperties::NewVpn(std::move(vpn));
        break;
      }
      mojom::ManagedStringPtr managed_type =
          GetManagedString(vpn_dict, ::onc::vpn::kType);
      CHECK(managed_type);
      vpn->type = OncVpnTypeToMojo(managed_type->active_value);

      vpn->auto_connect = GetManagedBoolean(vpn_dict, ::onc::vpn::kAutoConnect);
      vpn->host = GetManagedString(vpn_dict, ::onc::vpn::kHost);

      switch (vpn->type) {
        case mojom::VpnType::kL2TPIPsec:
          vpn->ip_sec = GetManagedIPSecProperties(vpn_dict, ::onc::vpn::kIPsec);
          vpn->l2tp = GetManagedL2TPProperties(vpn_dict, ::onc::vpn::kL2TP);
          break;
        case mojom::VpnType::kOpenVPN:
          vpn->open_vpn =
              GetManagedOpenVPNProperties(vpn_dict, ::onc::vpn::kOpenVPN);
          break;
        case mojom::VpnType::kExtension:
        case mojom::VpnType::kArc:
          const base::Value* third_party_dict =
              vpn_dict->FindKey(::onc::vpn::kThirdPartyVpn);
          if (third_party_dict) {
            vpn->provider_id = GetManagedString(
                third_party_dict, ::onc::third_party_vpn::kExtensionID);
            base::Optional<std::string> provider_name = GetString(
                third_party_dict, ::onc::third_party_vpn::kProviderName);
            if (provider_name)
              vpn->provider_name = *provider_name;
            if (vpn->provider_id && vpn->provider_name.empty()) {
              vpn->provider_name = GetVpnProviderName(
                  vpn_providers, vpn->provider_id->active_value);
            }
          } else {
            // Lookup VPN provider details from the NetworkState.
            const NetworkState::VpnProviderInfo* vpn_provider =
                network_state->vpn_provider();
            if (vpn_provider) {
              vpn->provider_id = mojom::ManagedString::New();
              vpn->provider_id->active_value = vpn_provider->id;
              vpn->provider_name =
                  GetVpnProviderName(vpn_providers, vpn_provider->id);
            }
          }
          break;
      }
      result->type_properties =
          mojom::NetworkTypeManagedProperties::NewVpn(std::move(vpn));
      break;
    }
    case mojom::NetworkType::kWiFi: {
      auto wifi = mojom::ManagedWiFiProperties::New();
      wifi->security = network_state->GetMojoSecurity();

      const base::Value* wifi_dict =
          GetDictionary(properties, ::onc::network_config::kWiFi);
      if (!wifi_dict) {
        result->type_properties =
            mojom::NetworkTypeManagedProperties::NewWifi(std::move(wifi));
        break;
      }
      wifi->allow_gateway_arp_polling =
          GetManagedBoolean(wifi_dict, ::onc::wifi::kAllowGatewayARPPolling);
      wifi->auto_connect =
          GetManagedBoolean(wifi_dict, ::onc::wifi::kAutoConnect);
      wifi->bssid = GetString(wifi_dict, ::onc::wifi::kBSSID);
      wifi->eap = GetManagedEAPProperties(wifi_dict, ::onc::wifi::kEAP);
      wifi->frequency = GetInt32(wifi_dict, ::onc::wifi::kFrequency);
      wifi->frequency_list =
          GetInt32List(wifi_dict, ::onc::wifi::kFrequencyList);
      wifi->ft_enabled = GetManagedBoolean(wifi_dict, ::onc::wifi::kFTEnabled);
      wifi->hex_ssid = GetManagedString(wifi_dict, ::onc::wifi::kHexSSID);
      wifi->hidden_ssid =
          GetManagedBoolean(wifi_dict, ::onc::wifi::kHiddenSSID);
      wifi->passphrase = GetManagedString(wifi_dict, ::onc::wifi::kPassphrase);
      wifi->roam_threshold =
          GetManagedInt32(wifi_dict, ::onc::wifi::kRoamThreshold);
      wifi->ssid = GetRequiredManagedString(wifi_dict, ::onc::wifi::kSSID);
      CHECK(wifi->ssid);
      wifi->signal_strength = GetInt32(wifi_dict, ::onc::wifi::kSignalStrength);
      wifi->tethering_state =
          GetString(wifi_dict, ::onc::wifi::kTetheringState);
      result->type_properties =
          mojom::NetworkTypeManagedProperties::NewWifi(std::move(wifi));
      break;
    }
    case mojom::NetworkType::kAll:
    case mojom::NetworkType::kMobile:
    case mojom::NetworkType::kWireless:
      NOTREACHED() << "NetworkStateProperties can not be of type: " << type;
      break;
  }

  return result;
}

bool NetworkTypeCanBeDisabled(mojom::NetworkType type) {
  switch (type) {
    case mojom::NetworkType::kCellular:
    case mojom::NetworkType::kTether:
    case mojom::NetworkType::kWiFi:
      return true;
    case mojom::NetworkType::kAll:
    case mojom::NetworkType::kEthernet:
    case mojom::NetworkType::kMobile:
    case mojom::NetworkType::kVPN:
    case mojom::NetworkType::kWireless:
      return false;
  }
  NOTREACHED();
  return false;
}

base::Value GetEAPProperties(const mojom::EAPConfigProperties& eap) {
  base::Value eap_dict(base::Value::Type::DICTIONARY);

  SetString(::onc::eap::kAnonymousIdentity, eap.anonymous_identity, &eap_dict);
  SetString(::onc::client_cert::kClientCertPKCS11Id, eap.client_cert_pkcs11_id,
            &eap_dict);
  SetString(::onc::client_cert::kClientCertType, eap.client_cert_type,
            &eap_dict);
  SetString(::onc::eap::kIdentity, eap.identity, &eap_dict);
  SetString(::onc::eap::kInner, eap.inner, &eap_dict);
  SetString(::onc::eap::kOuter, eap.outer, &eap_dict);
  SetString(::onc::eap::kPassword, eap.password, &eap_dict);
  eap_dict.SetBoolKey(::onc::eap::kSaveCredentials, eap.save_credentials);
  SetStringList(::onc::eap::kServerCAPEMs, eap.server_ca_pems, &eap_dict);
  SetString(::onc::eap::kSubjectMatch, eap.subject_match, &eap_dict);
  eap_dict.SetBoolKey(::onc::eap::kUseSystemCAs, eap.use_system_cas);

  return eap_dict;
}

std::unique_ptr<base::DictionaryValue> GetOncFromConfigProperties(
    const mojom::ConfigProperties* properties) {
  auto onc = std::make_unique<base::DictionaryValue>();

  // Process |properties->network_type| and set |type|. Configurations have only
  // one type dictionary.
  mojom::NetworkType type = mojom::NetworkType::kAll;  // Invalid type
  base::Value type_dict(base::Value::Type::DICTIONARY);

  if (properties->type_config->is_cellular()) {
    type = mojom::NetworkType::kCellular;
    const mojom::CellularConfigProperties& cellular =
        *properties->type_config->get_cellular();
    if (cellular.apn) {
      const mojom::ApnProperties& apn = *cellular.apn;
      base::Value apn_dict(base::Value::Type::DICTIONARY);
      apn_dict.SetStringKey(::onc::cellular_apn::kAccessPointName,
                            apn.access_point_name);
      SetString(::onc::cellular_apn::kAuthentication, apn.authentication,
                &apn_dict);
      SetString(::onc::cellular_apn::kLanguage, apn.language, &apn_dict);
      SetString(::onc::cellular_apn::kLocalizedName, apn.localized_name,
                &apn_dict);
      SetString(::onc::cellular_apn::kName, apn.name, &apn_dict);
      SetString(::onc::cellular_apn::kPassword, apn.password, &apn_dict);
      SetString(::onc::cellular_apn::kUsername, apn.username, &apn_dict);
      type_dict.SetKey(::onc::cellular::kAPN, std::move(apn_dict));
    }
  } else if (properties->type_config->is_ethernet()) {
    type = mojom::NetworkType::kEthernet;
    const mojom::EthernetConfigProperties& ethernet =
        *properties->type_config->get_ethernet();
    SetString(::onc::ethernet::kAuthentication, ethernet.authentication,
              &type_dict);
    if (ethernet.eap) {
      type_dict.SetKey(::onc::ethernet::kEAP,
                       GetEAPProperties(*ethernet.eap.get()));
    }
  } else if (properties->type_config->is_vpn()) {
    type = mojom::NetworkType::kVPN;
    const mojom::VPNConfigProperties& vpn = *properties->type_config->get_vpn();
    SetString(::onc::vpn::kHost, vpn.host, &type_dict);
    if (vpn.ip_sec) {
      const mojom::IPSecConfigProperties& ip_sec = *vpn.ip_sec;
      base::Value ip_sec_dict(base::Value::Type::DICTIONARY);
      SetString(::onc::ipsec::kAuthenticationType, ip_sec.authentication_type,
                &ip_sec_dict);
      SetString(::onc::client_cert::kClientCertPKCS11Id,
                ip_sec.client_cert_pkcs11_id, &ip_sec_dict);
      SetString(::onc::client_cert::kClientCertType, ip_sec.client_cert_type,
                &ip_sec_dict);
      SetString(::onc::ipsec::kGroup, ip_sec.group, &ip_sec_dict);
      ip_sec_dict.SetIntKey(::onc::ipsec::kIKEVersion, ip_sec.ike_version);
      SetString(::onc::ipsec::kPSK, ip_sec.psk, &ip_sec_dict);
      ip_sec_dict.SetBoolKey(::onc::l2tp::kSaveCredentials,
                             ip_sec.save_credentials);
      SetStringList(::onc::ipsec::kServerCAPEMs, ip_sec.server_ca_pems,
                    &ip_sec_dict);
      SetStringList(::onc::ipsec::kServerCARefs, ip_sec.server_ca_refs,
                    &ip_sec_dict);
      type_dict.SetKey(::onc::vpn::kIPsec, std::move(ip_sec_dict));
    }
    if (vpn.l2tp) {
      const mojom::L2TPConfigProperties& l2tp = *vpn.l2tp;
      base::Value l2tp_dict(base::Value::Type::DICTIONARY);
      l2tp_dict.SetBoolKey(::onc::l2tp::kLcpEchoDisabled,
                           l2tp.lcp_echo_disabled);
      SetString(::onc::l2tp::kPassword, l2tp.password, &l2tp_dict);
      l2tp_dict.SetBoolKey(::onc::l2tp::kSaveCredentials,
                           l2tp.save_credentials);
      SetString(::onc::l2tp::kUsername, l2tp.username, &l2tp_dict);
      type_dict.SetKey(::onc::vpn::kL2TP, std::move(l2tp_dict));
    }
    if (vpn.open_vpn) {
      const mojom::OpenVPNConfigProperties& open_vpn = *vpn.open_vpn;
      base::Value open_vpn_dict(base::Value::Type::DICTIONARY);
      SetString(::onc::client_cert::kClientCertPKCS11Id,
                open_vpn.client_cert_pkcs11_id, &open_vpn_dict);
      SetString(::onc::client_cert::kClientCertType, open_vpn.client_cert_type,
                &open_vpn_dict);
      SetStringList(::onc::openvpn::kExtraHosts, open_vpn.extra_hosts,
                    &open_vpn_dict);
      SetString(::onc::openvpn::kOTP, open_vpn.otp, &open_vpn_dict);
      SetString(::onc::openvpn::kPassword, open_vpn.password, &open_vpn_dict);
      open_vpn_dict.SetBoolKey(::onc::l2tp::kSaveCredentials,
                               open_vpn.save_credentials);
      SetStringList(::onc::openvpn::kServerCAPEMs, open_vpn.server_ca_pems,
                    &open_vpn_dict);
      SetStringList(::onc::openvpn::kServerCARefs, open_vpn.server_ca_refs,
                    &open_vpn_dict);
      SetString(::onc::vpn::kUsername, open_vpn.username, &open_vpn_dict);
      SetString(::onc::openvpn::kUserAuthenticationType,
                open_vpn.user_authentication_type, &open_vpn_dict);
      type_dict.SetKey(::onc::vpn::kOpenVPN, std::move(open_vpn_dict));
    }
    SetString(::onc::vpn::kType, MojoVpnTypeToOnc(vpn.type), &type_dict);
  } else if (properties->type_config->is_wifi()) {
    type = mojom::NetworkType::kWiFi;
    const mojom::WiFiConfigProperties& wifi =
        *properties->type_config->get_wifi();
    SetString(::onc::wifi::kPassphrase, wifi.passphrase, &type_dict);
    SetStringIfNotEmpty(::onc::wifi::kSSID, wifi.ssid, &type_dict);
    SetString(::onc::wifi::kPassphrase, wifi.passphrase, &type_dict);
    SetString(::onc::wifi::kSecurity, MojoSecurityTypeToOnc(wifi.security),
              &type_dict);
    if (wifi.eap) {
      type_dict.SetKey(::onc::wifi::kEAP, GetEAPProperties(*wifi.eap.get()));
    }
  }

  std::string onc_type = MojoNetworkTypeToOnc(type);
  if (onc_type.empty()) {
    NET_LOG(ERROR) << "Invalid NetworkConfig properties";
    return nullptr;
  }
  SetString(::onc::network_config::kType, onc_type, onc.get());

  // Process other |properties| members. Order matches the mojo struct.

  if (properties->ip_address_config_type) {
    onc->SetStringKey(::onc::network_config::kIPAddressConfigType,
                      *properties->ip_address_config_type);
  }

  SetString(::onc::network_config::kName, properties->name, onc.get());

  SetString(::onc::network_config::kNameServersConfigType,
            properties->name_servers_config_type, onc.get());

  if (properties->priority) {
    onc->SetIntKey(::onc::network_config::kPriority,
                   properties->priority->value);
  }

  if (properties->proxy_settings) {
    const mojom::ProxySettings& proxy = *properties->proxy_settings;
    base::Value proxy_dict(base::Value::Type::DICTIONARY);
    proxy_dict.SetStringKey(::onc::proxy::kType, proxy.type);
    if (proxy.manual) {
      const mojom::ManualProxySettings& manual = *proxy.manual;
      base::Value manual_dict(base::Value::Type::DICTIONARY);
      SetProxyLocation(::onc::proxy::kHttp, manual.http_proxy, &manual_dict);
      SetProxyLocation(::onc::proxy::kHttps, manual.secure_http_proxy,
                       &manual_dict);
      SetProxyLocation(::onc::proxy::kFtp, manual.ftp_proxy, &manual_dict);
      SetProxyLocation(::onc::proxy::kSocks, manual.socks, &manual_dict);
      proxy_dict.SetKey(::onc::proxy::kManual, std::move(manual_dict));
    }
    SetStringList(::onc::proxy::kExcludeDomains, proxy.exclude_domains,
                  &proxy_dict);
    SetString(::onc::proxy::kPAC, proxy.pac, &proxy_dict);
    onc->SetKey(::onc::network_config::kProxySettings, std::move(proxy_dict));
  }

  if (properties->static_ip_config) {
    const mojom::IPConfigProperties& ip_config = *properties->static_ip_config;
    base::Value ip_config_dict(base::Value::Type::DICTIONARY);
    SetString(::onc::ipconfig::kGateway, ip_config.gateway, &ip_config_dict);
    SetString(::onc::ipconfig::kIPAddress, ip_config.ip_address,
              &ip_config_dict);
    SetStringList(::onc::ipconfig::kNameServers, ip_config.name_servers,
                  &ip_config_dict);
    ip_config_dict.SetIntKey(::onc::ipconfig::kRoutingPrefix,
                             ip_config.routing_prefix);
    SetString(::onc::ipconfig::kType, ip_config.type, &ip_config_dict);
    SetString(::onc::ipconfig::kWebProxyAutoDiscoveryUrl,
              ip_config.web_proxy_auto_discovery_url, &ip_config_dict);
    onc->SetKey(::onc::network_config::kStaticIPConfig,
                std::move(ip_config_dict));
  }

  if (properties->auto_connect) {
    NetworkTypePattern type_pattern = MojoTypeToPattern(type);
    if (type_pattern.Equals(NetworkTypePattern::Cellular()) ||
        type_pattern.Equals(NetworkTypePattern::VPN()) ||
        type_pattern.Equals(NetworkTypePattern::WiFi())) {
      // Note: All type dicts use the same kAutoConnect key.
      type_dict.SetBoolKey(::onc::wifi::kAutoConnect,
                           properties->auto_connect->value);
    }
  }

  if (!type_dict.DictEmpty()) {
    onc->SetKey(onc_type, std::move(type_dict));
  }
  return onc;
}

mojom::NetworkCertificatePtr GetMojoCert(
    const NetworkCertificateHandler::Certificate& cert,
    mojom::CertificateType type) {
  auto result = mojom::NetworkCertificate::New();
  result->type = type;
  result->hash = cert.hash;
  result->issued_by = cert.issued_by;
  result->issued_to = cert.issued_to;
  result->hardware_backed = cert.hardware_backed;
  result->device_wide = cert.device_wide;
  if (type == mojom::CertificateType::kServerCA)
    result->pem_or_id = cert.pem;
  if (type == mojom::CertificateType::kUserCert)
    result->pem_or_id = cert.pkcs11_id;
  return result;
}

}  // namespace

CrosNetworkConfig::CrosNetworkConfig()
    : CrosNetworkConfig(
          NetworkHandler::Get()->network_state_handler(),
          NetworkHandler::Get()->network_device_handler(),
          NetworkHandler::Get()->managed_network_configuration_handler(),
          NetworkHandler::Get()->network_connection_handler(),
          NetworkHandler::Get()->network_certificate_handler()) {}

CrosNetworkConfig::CrosNetworkConfig(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler,
    ManagedNetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkCertificateHandler* network_certificate_handler)
    : network_state_handler_(network_state_handler),
      network_device_handler_(network_device_handler),
      network_configuration_handler_(network_configuration_handler),
      network_connection_handler_(network_connection_handler),
      network_certificate_handler_(network_certificate_handler) {
  CHECK(network_state_handler);
}

CrosNetworkConfig::~CrosNetworkConfig() {
  if (network_state_handler_->HasObserver(this))
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (network_certificate_handler_ &&
      network_certificate_handler_->HasObserver(this)) {
    network_certificate_handler_->RemoveObserver(this);
  }
}

void CrosNetworkConfig::BindReceiver(
    mojo::PendingReceiver<mojom::CrosNetworkConfig> receiver) {
  NET_LOG(EVENT) << "CrosNetworkConfig::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void CrosNetworkConfig::AddObserver(
    mojo::PendingRemote<mojom::CrosNetworkConfigObserver> observer) {
  if (!network_state_handler_->HasObserver(this))
    network_state_handler_->AddObserver(this, FROM_HERE);
  if (network_certificate_handler_ &&
      !network_certificate_handler_->HasObserver(this)) {
    network_certificate_handler_->AddObserver(this);
  }
  observers_.Add(std::move(observer));
}

void CrosNetworkConfig::GetNetworkState(const std::string& guid,
                                        GetNetworkStateCallback callback) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network) {
    NET_LOG(ERROR) << "Network not found: " << guid;
    std::move(callback).Run(nullptr);
    return;
  }
  if (network->type() == shill::kTypeEthernetEap) {
    NET_LOG(ERROR) << "EthernetEap not supported for GetNetworkState";
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      NetworkStateToMojo(network_state_handler_, vpn_providers_, network));
}

void CrosNetworkConfig::GetNetworkStateList(
    mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  NetworkStateHandler::NetworkStateList networks;
  NetworkTypePattern pattern = MojoTypeToPattern(filter->network_type);
  switch (filter->filter) {
    case mojom::FilterType::kActive:
      network_state_handler_->GetActiveNetworkListByType(pattern, &networks);
      if (filter->limit > 0 &&
          static_cast<int>(networks.size()) > filter->limit)
        networks.resize(filter->limit);
      break;
    case mojom::FilterType::kVisible:
      network_state_handler_->GetNetworkListByType(
          pattern, /*configured_only=*/false, /*visible_only=*/true,
          filter->limit, &networks);
      break;
    case mojom::FilterType::kConfigured:
      network_state_handler_->GetNetworkListByType(
          pattern, /*configured_only=*/true, /*visible_only=*/false,
          filter->limit, &networks);
      break;
    case mojom::FilterType::kAll:
      network_state_handler_->GetNetworkListByType(
          pattern, /*configured_only=*/false, /*visible_only=*/false,
          filter->limit, &networks);
      break;
  }
  std::vector<mojom::NetworkStatePropertiesPtr> result;
  for (const NetworkState* network : networks) {
    if (network->type() == shill::kTypeEthernetEap) {
      // EthernetEap is used by Shill to store EAP properties and does not
      // represent a separate network service.
      continue;
    }
    mojom::NetworkStatePropertiesPtr mojo_network =
        NetworkStateToMojo(network_state_handler_, vpn_providers_, network);
    if (mojo_network)
      result.emplace_back(std::move(mojo_network));
  }
  std::move(callback).Run(std::move(result));
}

void CrosNetworkConfig::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  NetworkStateHandler::DeviceStateList devices;
  network_state_handler_->GetDeviceList(&devices);
  std::vector<mojom::DeviceStatePropertiesPtr> result;
  for (const DeviceState* device : devices) {
    mojom::DeviceStateType technology_state =
        GetMojoDeviceStateType(network_state_handler_->GetTechnologyState(
            NetworkTypePattern::Primitive(device->type())));
    if (technology_state == mojom::DeviceStateType::kUnavailable) {
      NET_LOG(ERROR) << "Device state unavailable: " << device->name();
      continue;
    }
    mojom::DeviceStatePropertiesPtr mojo_device =
        DeviceStateToMojo(device, technology_state);
    if (mojo_device)
      result.emplace_back(std::move(mojo_device));
  }
  std::move(callback).Run(std::move(result));
}

void CrosNetworkConfig::GetManagedProperties(
    const std::string& guid,
    GetManagedPropertiesCallback callback) {
  if (!network_configuration_handler_) {
    NET_LOG(ERROR) << "GetManagedProperties called with no handler";
    std::move(callback).Run(nullptr);
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network) {
    NET_LOG(ERROR) << "Network not found: " << guid;
    std::move(callback).Run(nullptr);
    return;
  }

  int callback_id = callback_id_++;
  get_managed_properties_callbacks_[callback_id] = std::move(callback);

  network_configuration_handler_->GetManagedProperties(
      chromeos::LoginState::Get()->primary_user_hash(), network->path(),
      base::Bind(&CrosNetworkConfig::GetManagedPropertiesSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::GetManagedPropertiesFailure,
                 weak_factory_.GetWeakPtr(), guid, callback_id));
}

void CrosNetworkConfig::GetManagedPropertiesSuccess(
    int callback_id,
    const std::string& service_path,
    const base::DictionaryValue& properties) {
  auto iter = get_managed_properties_callbacks_.find(callback_id);
  DCHECK(iter != get_managed_properties_callbacks_.end());
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state) {
    NET_LOG(ERROR) << "Network not found: " << service_path;
    std::move(iter->second).Run(nullptr);
    return;
  }
  mojom::ManagedPropertiesPtr managed_properties =
      ManagedPropertiesToMojo(network_state, vpn_providers_, &properties);

  // For Ethernet networks with no authentication, check for a separate
  // EthernetEAP configuration.
  const NetworkState* eap_state = nullptr;
  if (managed_properties->type == mojom::NetworkType::kEthernet) {
    mojom::ManagedEthernetPropertiesPtr& ethernet =
        managed_properties->type_properties->get_ethernet();
    if (!ethernet->authentication || ethernet->authentication->active_value ==
                                         ::onc::ethernet::kAuthenticationNone) {
      eap_state = network_state_handler_->GetEAPForEthernet(
          network_state->path(), /*connected_only=*/false);
    }
  }
  if (!eap_state) {
    // No EAP properties, return the managed properties as-is.
    std::move(iter->second).Run(std::move(managed_properties));
    get_managed_properties_callbacks_.erase(iter);
    return;
  }

  // Request the EAP state. On success the EAP state will be applied to
  // |managed_properties| and returned. On failure |managed_properties| will
  // be returned as-is.
  NET_LOG(DEBUG) << "Requesting EAP state for: " + service_path
                 << " from: " << eap_state->path();
  managed_properties_[callback_id] = std::move(managed_properties);
  network_configuration_handler_->GetManagedProperties(
      chromeos::LoginState::Get()->primary_user_hash(), eap_state->path(),
      base::Bind(&CrosNetworkConfig::GetManagedPropertiesSuccessEap,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::GetManagedPropertiesSuccessNoEap,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void CrosNetworkConfig::GetManagedPropertiesSuccessEap(
    int callback_id,
    const std::string& service_path,
    const base::DictionaryValue& eap_properties) {
  auto iter = get_managed_properties_callbacks_.find(callback_id);
  DCHECK(iter != get_managed_properties_callbacks_.end());

  auto properties_iter = managed_properties_.find(callback_id);
  DCHECK(properties_iter != managed_properties_.end());
  mojom::ManagedPropertiesPtr managed_properties =
      std::move(properties_iter->second);
  managed_properties_.erase(properties_iter);

  // Copy the EAP properties to |managed_properties_| before sending.
  const base::Value* ethernet_dict =
      GetDictionary(&eap_properties, ::onc::network_config::kEthernet);
  if (ethernet_dict) {
    auto ethernet = mojom::ManagedEthernetProperties::New();
    ethernet->authentication =
        GetManagedString(ethernet_dict, ::onc::ethernet::kAuthentication);
    ethernet->eap =
        GetManagedEAPProperties(ethernet_dict, ::onc::ethernet::kEAP);
    managed_properties->type_properties =
        mojom::NetworkTypeManagedProperties::NewEthernet(std::move(ethernet));
  }

  std::move(iter->second).Run(std::move(managed_properties));
  get_managed_properties_callbacks_.erase(iter);
}

void CrosNetworkConfig::GetManagedPropertiesSuccessNoEap(
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = get_managed_properties_callbacks_.find(callback_id);
  DCHECK(iter != get_managed_properties_callbacks_.end());

  auto properties_iter = managed_properties_.find(callback_id);
  DCHECK(properties_iter != managed_properties_.end());
  mojom::ManagedPropertiesPtr managed_properties =
      std::move(properties_iter->second);
  managed_properties_.erase(properties_iter);

  // No EAP properties, send the unmodified managed_properties_.
  std::move(iter->second).Run(std::move(managed_properties));
  get_managed_properties_callbacks_.erase(iter);
}

void CrosNetworkConfig::GetManagedPropertiesFailure(
    std::string guid,
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = get_managed_properties_callbacks_.find(callback_id);
  DCHECK(iter != get_managed_properties_callbacks_.end());
  NET_LOG(ERROR) << "Failed to get network properties: " << guid
                 << " Error: " << error_name;
  std::move(iter->second).Run(nullptr);
  get_managed_properties_callbacks_.erase(iter);
}

void CrosNetworkConfig::SetProperties(const std::string& guid,
                                      mojom::ConfigPropertiesPtr properties,
                                      SetPropertiesCallback callback) {
  if (!network_configuration_handler_) {
    NET_LOG(ERROR) << "SetProperties called with no handler";
    std::move(callback).Run(false, kErrorNotReady);
    return;
  }
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network || network->profile_path().empty()) {
    NET_LOG(ERROR) << "SetProperties called with unconfigured network: "
                   << guid;
    std::move(callback).Run(false, kErrorNetworkUnavailable);
    return;
  }

  // If EthernetEAP properties exist, and properties.ethernet.eap is provided,
  // apply the configuration to the EthernetEAP service. Currently we only
  // support configuration of EAP properties or other properties (e.g IP
  // Config), not both.
  if (network->type() == shill::kTypeEthernet &&
      properties->type_config->is_ethernet() &&
      properties->type_config->get_ethernet()->eap) {
    const NetworkState* eap_state = network_state_handler_->GetEAPForEthernet(
        network->path(), /*connected_only=*/false);
    if (!eap_state) {
      NET_LOG(ERROR)
          << "SetProperties called with ethernet.eap but no EAP config: "
          << guid;
      std::move(callback).Run(false, kErrorNetworkUnavailable);
    }
    network = eap_state;
  }

  std::unique_ptr<base::DictionaryValue> onc =
      GetOncFromConfigProperties(properties.get());
  if (!onc) {
    NET_LOG(ERROR) << "Bad ONC Configuration for " << guid;
    std::move(callback).Run(false, kErrorInvalidONCConfiguration);
    return;
  }

  NET_LOG(DEBUG) << "Configuring properties for " << guid << ": " << *onc;

  int callback_id = callback_id_++;
  set_properties_callbacks_[callback_id] = std::move(callback);

  // If the profile path is empty the network is not saved, so we need to call
  // CreateConfiguration(). This can happen for EthernetEAP where a default
  // service is generated by Shill but may not be saved.
  if (network->profile_path().empty()) {
    NET_LOG(USER) << "Configuring properties for " << guid
                  << " (no profile entry set)";
    std::string user_id_hash = LoginState::Get()->primary_user_hash();
    network_configuration_handler_->CreateConfiguration(
        user_id_hash, *onc,
        base::Bind(&CrosNetworkConfig::SetPropertiesConfigureSuccess,
                   weak_factory_.GetWeakPtr(), callback_id),
        base::Bind(&CrosNetworkConfig::SetPropertiesFailure,
                   weak_factory_.GetWeakPtr(), guid, callback_id));
    return;
  }

  network_configuration_handler_->SetProperties(
      network->path(), *onc,
      base::Bind(&CrosNetworkConfig::SetPropertiesSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::SetPropertiesFailure,
                 weak_factory_.GetWeakPtr(), guid, callback_id));
}

void CrosNetworkConfig::SetPropertiesSuccess(int callback_id) {
  auto iter = set_properties_callbacks_.find(callback_id);
  DCHECK(iter != set_properties_callbacks_.end());
  std::move(iter->second).Run(/*success=*/true, /*message=*/"");
  set_properties_callbacks_.erase(iter);
}

void CrosNetworkConfig::SetPropertiesConfigureSuccess(
    int callback_id,
    const std::string& service_path,
    const std::string& guid) {
  auto iter = set_properties_callbacks_.find(callback_id);
  DCHECK(iter != set_properties_callbacks_.end());
  std::move(iter->second).Run(/*success=*/true, /*message=*/"");
  set_properties_callbacks_.erase(iter);
}

void CrosNetworkConfig::SetPropertiesFailure(
    const std::string& guid,
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = set_properties_callbacks_.find(callback_id);
  DCHECK(iter != set_properties_callbacks_.end());
  NET_LOG(ERROR) << "Failed to set network properties: " << guid
                 << " Error: " << error_name;
  std::move(iter->second).Run(/*success=*/false, error_name);
  set_properties_callbacks_.erase(iter);
}

void CrosNetworkConfig::ConfigureNetwork(mojom::ConfigPropertiesPtr properties,
                                         bool shared,
                                         ConfigureNetworkCallback callback) {
  if (!network_configuration_handler_) {
    NET_LOG(ERROR) << "Configure called with no handler";
    std::move(callback).Run(/*guid=*/base::nullopt, kErrorNotReady);
    return;
  }

  if (!shared && UserManager::Get()->GetPrimaryUser() !=
                     UserManager::Get()->GetActiveUser()) {
    NET_LOG(ERROR)
        << "Attempt to set unshared configuration from non primary user";
    std::move(callback).Run(/*guid=*/base::nullopt, kErrorAccessToSharedConfig);
  }

  std::unique_ptr<base::DictionaryValue> onc =
      GetOncFromConfigProperties(properties.get());
  if (!onc) {
    std::move(callback).Run(/*guid=*/base::nullopt,
                            kErrorInvalidONCConfiguration);
    return;
  }

  std::string user_id_hash =
      shared ? "" : LoginState::Get()->primary_user_hash();

  int callback_id = callback_id_++;
  configure_network_callbacks_[callback_id] = std::move(callback);

  network_configuration_handler_->CreateConfiguration(
      user_id_hash, *onc,
      base::Bind(&CrosNetworkConfig::ConfigureNetworkSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::ConfigureNetworkFailure,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void CrosNetworkConfig::ConfigureNetworkSuccess(int callback_id,
                                                const std::string& service_path,
                                                const std::string& guid) {
  auto iter = configure_network_callbacks_.find(callback_id);
  DCHECK(iter != configure_network_callbacks_.end());
  std::move(iter->second).Run(guid, /*message=*/"");
  configure_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::ConfigureNetworkFailure(
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = configure_network_callbacks_.find(callback_id);
  DCHECK(iter != configure_network_callbacks_.end());
  DCHECK(iter->second);
  NET_LOG(ERROR) << "Failed to configure network. Error: " << error_name;
  std::move(iter->second).Run(/*guid=*/base::nullopt, error_name);
  configure_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::ForgetNetwork(const std::string& guid,
                                      ForgetNetworkCallback callback) {
  if (!network_configuration_handler_) {
    NET_LOG(ERROR) << "ForgetNetwork called with no handler";
    std::move(callback).Run(false);
    return;
  }
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network || network->profile_path().empty()) {
    NET_LOG(ERROR) << "ForgetNetwork called with unconfigured network: "
                   << guid;
    std::move(callback).Run(false);
    return;
  }

  bool allow_forget_shared_config = true;
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_UNKNOWN;
  std::string user_id_hash = LoginState::Get()->primary_user_hash();
  if (network_configuration_handler_->FindPolicyByGUID(user_id_hash, guid,
                                                       &onc_source)) {
    if (onc_source == ::onc::ONC_SOURCE_USER_POLICY) {
      // Prevent a policy controlled configuration removal.
      std::move(callback).Run(false);
      return;
    }
    if (onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY)
      allow_forget_shared_config = false;
  }

  int callback_id = callback_id_++;
  forget_network_callbacks_[callback_id] = std::move(callback);

  if (allow_forget_shared_config) {
    network_configuration_handler_->RemoveConfiguration(
        network->path(),
        base::Bind(&CrosNetworkConfig::ForgetNetworkSuccess,
                   weak_factory_.GetWeakPtr(), callback_id),
        base::Bind(&CrosNetworkConfig::ForgetNetworkFailure,
                   weak_factory_.GetWeakPtr(), guid, callback_id));
  } else {
    network_configuration_handler_->RemoveConfigurationFromCurrentProfile(
        network->path(),
        base::Bind(&CrosNetworkConfig::ForgetNetworkSuccess,
                   weak_factory_.GetWeakPtr(), callback_id),
        base::Bind(&CrosNetworkConfig::ForgetNetworkFailure,
                   weak_factory_.GetWeakPtr(), guid, callback_id));
  }
}

void CrosNetworkConfig::ForgetNetworkSuccess(int callback_id) {
  auto iter = forget_network_callbacks_.find(callback_id);
  DCHECK(iter != forget_network_callbacks_.end());
  std::move(iter->second).Run(/*success=*/true);
  forget_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::ForgetNetworkFailure(
    const std::string& guid,
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = forget_network_callbacks_.find(callback_id);
  DCHECK(iter != forget_network_callbacks_.end());
  NET_LOG(ERROR) << "Failed to forget network: " << guid
                 << " Error: " << error_name;
  std::move(iter->second).Run(/*success=*/false);
  forget_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::SetNetworkTypeEnabledState(
    mojom::NetworkType type,
    bool enabled,
    SetNetworkTypeEnabledStateCallback callback) {
  if (!NetworkTypeCanBeDisabled(type)) {
    std::move(callback).Run(false);
    return;
  }
  NetworkTypePattern pattern = MojoTypeToPattern(type);
  if (!network_state_handler_->IsTechnologyAvailable(pattern)) {
    NET_LOG(ERROR) << "Technology unavailable: " << type;
    std::move(callback).Run(false);
    return;
  }
  if (network_state_handler_->IsTechnologyProhibited(pattern)) {
    NET_LOG(ERROR) << "Technology prohibited: " << type;
    std::move(callback).Run(false);
    return;
  }
  // Set the technology enabled state and return true. The call to Shill does
  // not have a 'success' callback (and errors are already logged).
  network_state_handler_->SetTechnologyEnabled(
      pattern, enabled, chromeos::network_handler::ErrorCallback());
  std::move(callback).Run(true);
}

void CrosNetworkConfig::SetCellularSimState(
    mojom::CellularSimStatePtr sim_state,
    SetCellularSimStateCallback callback) {
  const DeviceState* device_state =
      network_state_handler_->GetDeviceStateByType(
          NetworkTypePattern::Cellular());
  if (!device_state || device_state->IsSimAbsent()) {
    std::move(callback).Run(false);
    return;
  }

  const std::string& lock_type = device_state->sim_lock_type();

  // When unblocking a PUK locked SIM, a new PIN must be provided.
  if (lock_type == shill::kSIMLockPuk && !sim_state->new_pin) {
    NET_LOG(ERROR) << "SetCellularSimState: PUK locked and no pin provided.";
    std::move(callback).Run(false);
    return;
  }

  int callback_id = callback_id_++;
  set_cellular_sim_state_callbacks_[callback_id] = std::move(callback);

  if (lock_type == shill::kSIMLockPuk) {
    // Unblock a PUK locked SIM.
    network_device_handler_->UnblockPin(
        device_state->path(), sim_state->current_pin_or_puk,
        *sim_state->new_pin,
        base::Bind(&CrosNetworkConfig::SetCellularSimStateSuccess,
                   weak_factory_.GetWeakPtr(), callback_id),
        base::Bind(&CrosNetworkConfig::SetCellularSimStateFailure,
                   weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  if (lock_type == shill::kSIMLockPin) {
    // Unlock locked SIM.
    network_device_handler_->EnterPin(
        device_state->path(), sim_state->current_pin_or_puk,
        base::Bind(&CrosNetworkConfig::SetCellularSimStateSuccess,
                   weak_factory_.GetWeakPtr(), callback_id),
        base::Bind(&CrosNetworkConfig::SetCellularSimStateFailure,
                   weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  if (sim_state->new_pin) {
    // Change the SIM PIN.
    network_device_handler_->ChangePin(
        device_state->path(), sim_state->current_pin_or_puk,
        *sim_state->new_pin,
        base::Bind(&CrosNetworkConfig::SetCellularSimStateSuccess,
                   weak_factory_.GetWeakPtr(), callback_id),
        base::Bind(&CrosNetworkConfig::SetCellularSimStateFailure,
                   weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  // Enable or disable SIM locking.
  network_device_handler_->RequirePin(
      device_state->path(), sim_state->require_pin,
      sim_state->current_pin_or_puk,
      base::Bind(&CrosNetworkConfig::SetCellularSimStateSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::SetCellularSimStateFailure,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void CrosNetworkConfig::SetCellularSimStateSuccess(int callback_id) {
  auto iter = set_cellular_sim_state_callbacks_.find(callback_id);
  DCHECK(iter != set_cellular_sim_state_callbacks_.end());
  std::move(iter->second).Run(true);
  set_cellular_sim_state_callbacks_.erase(iter);
}

void CrosNetworkConfig::SetCellularSimStateFailure(
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = set_cellular_sim_state_callbacks_.find(callback_id);
  DCHECK(iter != set_cellular_sim_state_callbacks_.end());
  std::move(iter->second).Run(false);
  set_cellular_sim_state_callbacks_.erase(iter);
}

void CrosNetworkConfig::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    SelectCellularMobileNetworkCallback callback) {
  const DeviceState* device_state = nullptr;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (network_state) {
    device_state =
        network_state_handler_->GetDeviceState(network_state->device_path());
  }
  if (!device_state) {
    std::move(callback).Run(false);
    return;
  }

  int callback_id = callback_id_++;
  select_cellular_mobile_network_callbacks_[callback_id] = std::move(callback);

  network_device_handler_->RegisterCellularNetwork(
      device_state->path(), network_id,
      base::Bind(&CrosNetworkConfig::SelectCellularMobileNetworkSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::SelectCellularMobileNetworkFailure,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void CrosNetworkConfig::SelectCellularMobileNetworkSuccess(int callback_id) {
  auto iter = select_cellular_mobile_network_callbacks_.find(callback_id);
  DCHECK(iter != select_cellular_mobile_network_callbacks_.end());
  std::move(iter->second).Run(true);
  select_cellular_mobile_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::SelectCellularMobileNetworkFailure(
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = select_cellular_mobile_network_callbacks_.find(callback_id);
  DCHECK(iter != select_cellular_mobile_network_callbacks_.end());
  std::move(iter->second).Run(false);
  select_cellular_mobile_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::RequestNetworkScan(mojom::NetworkType type) {
  network_state_handler_->RequestScan(MojoTypeToPattern(type));
}

void CrosNetworkConfig::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  auto result = mojom::GlobalPolicy::New();
  // Global network configuration policy values come from the device policy.
  const base::DictionaryValue* global_policy_dict =
      network_configuration_handler_->GetGlobalConfigFromPolicy(
          /*userhash=*/std::string());
  if (global_policy_dict) {
    result->allow_only_policy_networks_to_autoconnect = GetBoolean(
        global_policy_dict,
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect);
    result->allow_only_policy_networks_to_connect = GetBoolean(
        global_policy_dict,
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect);
    result->allow_only_policy_networks_to_connect_if_available = GetBoolean(
        global_policy_dict, ::onc::global_network_config::
                                kAllowOnlyPolicyNetworksToConnectIfAvailable);
    base::Optional<std::vector<std::string>> blocked_hex_ssids = GetStringList(
        global_policy_dict, ::onc::global_network_config::kBlacklistedHexSSIDs);
    if (blocked_hex_ssids)
      result->blocked_hex_ssids = std::move(*blocked_hex_ssids);
  }
  std::move(callback).Run(std::move(result));
}

void CrosNetworkConfig::StartConnect(const std::string& guid,
                                     StartConnectCallback callback) {
  std::string service_path = GetServicePathFromGuid(guid);
  if (service_path.empty()) {
    std::move(callback).Run(mojom::StartConnectResult::kInvalidGuid,
                            NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  int callback_id = callback_id_++;
  start_connect_callbacks_[callback_id] = std::move(callback);

  network_connection_handler_->ConnectToNetwork(
      service_path,
      base::Bind(&CrosNetworkConfig::StartConnectSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::StartConnectFailure,
                 weak_factory_.GetWeakPtr(), callback_id),
      true /* check_error_state */, chromeos::ConnectCallbackMode::ON_STARTED);
}

void CrosNetworkConfig::StartConnectSuccess(int callback_id) {
  auto iter = start_connect_callbacks_.find(callback_id);
  DCHECK(iter != start_connect_callbacks_.end());
  std::move(iter->second)
      .Run(mojom::StartConnectResult::kSuccess, std::string());
  start_connect_callbacks_.erase(iter);
}

void CrosNetworkConfig::StartConnectFailure(
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = start_connect_callbacks_.find(callback_id);
  DCHECK(iter != start_connect_callbacks_.end());
  mojom::StartConnectResult result;
  if (error_name == NetworkConnectionHandler::kErrorNotFound) {
    result = mojom::StartConnectResult::kInvalidGuid;
  } else if (error_name == NetworkConnectionHandler::kErrorConnected ||
             error_name == NetworkConnectionHandler::kErrorConnecting) {
    result = mojom::StartConnectResult::kInvalidState;
  } else if (error_name == NetworkConnectionHandler::kErrorConnectCanceled) {
    result = mojom::StartConnectResult::kCanceled;
  } else if (error_name == NetworkConnectionHandler::kErrorPassphraseRequired ||
             error_name == NetworkConnectionHandler::kErrorBadPassphrase ||
             error_name ==
                 NetworkConnectionHandler::kErrorCertificateRequired ||
             error_name ==
                 NetworkConnectionHandler::kErrorConfigurationRequired ||
             error_name ==
                 NetworkConnectionHandler::kErrorAuthenticationRequired ||
             error_name == NetworkConnectionHandler::kErrorCertLoadTimeout ||
             error_name == NetworkConnectionHandler::kErrorConfigureFailed) {
    result = mojom::StartConnectResult::kNotConfigured;
  } else if (error_name == NetworkConnectionHandler::kErrorBlockedByPolicy) {
    result = mojom::StartConnectResult::kBlocked;
  } else {
    result = mojom::StartConnectResult::kUnknown;
  }
  std::move(iter->second).Run(result, error_name);
  start_connect_callbacks_.erase(iter);
}

void CrosNetworkConfig::StartDisconnect(const std::string& guid,
                                        StartDisconnectCallback callback) {
  std::string service_path = GetServicePathFromGuid(guid);
  if (service_path.empty()) {
    std::move(callback).Run(false);
    return;
  }

  int callback_id = callback_id_++;
  start_disconnect_callbacks_[callback_id] = std::move(callback);

  network_connection_handler_->DisconnectNetwork(
      service_path,
      base::Bind(&CrosNetworkConfig::StartDisconnectSuccess,
                 weak_factory_.GetWeakPtr(), callback_id),
      base::Bind(&CrosNetworkConfig::StartDisconnectFailure,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void CrosNetworkConfig::StartDisconnectSuccess(int callback_id) {
  auto iter = start_disconnect_callbacks_.find(callback_id);
  DCHECK(iter != start_disconnect_callbacks_.end());
  std::move(iter->second).Run(true);
  start_disconnect_callbacks_.erase(iter);
}

void CrosNetworkConfig::StartDisconnectFailure(
    int callback_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  auto iter = start_disconnect_callbacks_.find(callback_id);
  DCHECK(iter != start_disconnect_callbacks_.end());
  std::move(iter->second).Run(false);
  start_disconnect_callbacks_.erase(iter);
}

void CrosNetworkConfig::SetVpnProviders(
    std::vector<mojom::VpnProviderPtr> providers) {
  vpn_providers_ = std::move(providers);
  for (auto& observer : observers_)
    observer->OnVpnProvidersChanged();
}

void CrosNetworkConfig::GetVpnProviders(GetVpnProvidersCallback callback) {
  std::move(callback).Run(mojo::Clone(vpn_providers_));
}

void CrosNetworkConfig::GetNetworkCertificates(
    GetNetworkCertificatesCallback callback) {
  const std::vector<NetworkCertificateHandler::Certificate>&
      handler_server_cas =
          network_certificate_handler_->server_ca_certificates();
  std::vector<mojom::NetworkCertificatePtr> server_cas;
  for (const auto& cert : handler_server_cas)
    server_cas.push_back(GetMojoCert(cert, mojom::CertificateType::kServerCA));

  std::vector<mojom::NetworkCertificatePtr> user_certs;
  const std::vector<NetworkCertificateHandler::Certificate>&
      handler_user_certs = network_certificate_handler_->client_certificates();
  for (const auto& cert : handler_user_certs)
    user_certs.push_back(GetMojoCert(cert, mojom::CertificateType::kUserCert));

  std::move(callback).Run(std::move(server_cas), std::move(user_certs));
}

// NetworkStateHandlerObserver
void CrosNetworkConfig::NetworkListChanged() {
  for (auto& observer : observers_)
    observer->OnNetworkStateListChanged();
}

void CrosNetworkConfig::DeviceListChanged() {
  for (auto& observer : observers_)
    observer->OnDeviceStateListChanged();
}

void CrosNetworkConfig::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  std::vector<mojom::NetworkStatePropertiesPtr> result;
  for (const NetworkState* network : active_networks) {
    mojom::NetworkStatePropertiesPtr mojo_network =
        NetworkStateToMojo(network_state_handler_, vpn_providers_, network);
    if (mojo_network)
      result.emplace_back(std::move(mojo_network));
  }
  for (auto& observer : observers_)
    observer->OnActiveNetworksChanged(mojo::Clone(result));
}

void CrosNetworkConfig::NetworkPropertiesUpdated(const NetworkState* network) {
  if (network->type() == shill::kTypeEthernetEap)
    return;
  mojom::NetworkStatePropertiesPtr mojo_network =
      NetworkStateToMojo(network_state_handler_, vpn_providers_, network);
  if (!mojo_network)
    return;
  for (auto& observer : observers_)
    observer->OnNetworkStateChanged(mojo_network.Clone());
}

void CrosNetworkConfig::DevicePropertiesUpdated(const DeviceState* device) {
  DeviceListChanged();
}

void CrosNetworkConfig::OnShuttingDown() {
  if (network_state_handler_->HasObserver(this))
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

void CrosNetworkConfig::OnCertificatesChanged() {
  for (auto& observer : observers_)
    observer->OnNetworkCertificatesChanged();
}

const std::string& CrosNetworkConfig::GetServicePathFromGuid(
    const std::string& guid) {
  const chromeos::NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  return network ? network->path() : base::EmptyString();
}

}  // namespace network_config
}  // namespace chromeos
