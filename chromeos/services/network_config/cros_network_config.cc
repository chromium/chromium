// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/cros_network_config.h"

#include <cmath>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_name_util.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/onc/onc_translation_tables.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/sync_wifi/network_eligibility_checker.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config_mojom_traits.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/ip_address.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using user_manager::UserManager;

namespace chromeos {
namespace network_config {

namespace {

// TODO(https://crbug.com/1164001): remove after migrating to ash.
using ::ash::LoginState;
namespace sync_wifi {
using ::ash::sync_wifi::IsEligibleForSync;
}

// Error strings from networking_private_api.cc. TODO(1004434): Enumerate
// these in mojo.
const char kErrorAccessToSharedConfig[] = "Error.CannotChangeSharedConfig";
const char kErrorInvalidONCConfiguration[] = "Error.InvalidONCConfiguration";
const char kErrorNetworkUnavailable[] = "Error.NetworkUnavailable";
const char kErrorNotReady[] = "Error.NotReady";

// Default traffic counter reset day.
const int kDefaultResetDay = 1;

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

mojom::IPConfigType OncIPConfigTypeToMojo(const std::string& ip_config_type) {
  if (ip_config_type == ::onc::ipconfig::kIPv4)
    return mojom::IPConfigType::kIPv4;
  if (ip_config_type == ::onc::ipconfig::kIPv6)
    return mojom::IPConfigType::kIPv6;
  NOTREACHED() << "Unsupported ONC IPConfig type: " << ip_config_type;
  return mojom::IPConfigType::kIPv4;
}

std::string MojoIPConfigTypeToOnc(mojom::IPConfigType type) {
  switch (type) {
    case mojom::IPConfigType::kIPv4:
      return ::onc::ipconfig::kIPv4;
    case mojom::IPConfigType::kIPv6:
      return ::onc::ipconfig::kIPv6;
  }
  NOTREACHED() << "Unexpected mojo IPConfig type: " << type;
  return ::onc::ipconfig::kIPv4;
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
    auto portal_state = network->GetPortalState();
    switch (portal_state) {
      case NetworkState::PortalState::kUnknown:
        return mojom::ConnectionStateType::kConnected;
      case NetworkState::PortalState::kOnline:
        return mojom::ConnectionStateType::kOnline;
      case NetworkState::PortalState::kPortalSuspected:
      case NetworkState::PortalState::kPortal:
      case NetworkState::PortalState::kProxyAuthRequired:
      case NetworkState::PortalState::kNoInternet:
        // See PortalState for differentiation of portal states.
        return mojom::ConnectionStateType::kPortal;
    }
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
  if (onc_vpn_type == ::onc::vpn::kIPsec)
    return mojom::VpnType::kIKEv2;
  if (onc_vpn_type == ::onc::vpn::kTypeL2TP_IPsec)
    return mojom::VpnType::kL2TPIPsec;
  if (onc_vpn_type == ::onc::vpn::kOpenVPN)
    return mojom::VpnType::kOpenVPN;
  if (onc_vpn_type == ::onc::vpn::kWireGuard)
    return mojom::VpnType::kWireGuard;
  if (onc_vpn_type == ::onc::vpn::kThirdPartyVpn)
    return mojom::VpnType::kExtension;
  if (onc_vpn_type == ::onc::vpn::kArcVpn)
    return mojom::VpnType::kArc;
  NOTREACHED() << "Unsupported ONC VPN type: " << onc_vpn_type;
  return mojom::VpnType::kOpenVPN;
}

std::string MojoVpnTypeToOnc(mojom::VpnType mojo_vpn_type) {
  switch (mojo_vpn_type) {
    case mojom::VpnType::kIKEv2:
      return ::onc::vpn::kIPsec;
    case mojom::VpnType::kL2TPIPsec:
      return ::onc::vpn::kTypeL2TP_IPsec;
    case mojom::VpnType::kOpenVPN:
      return ::onc::vpn::kOpenVPN;
    case mojom::VpnType::kWireGuard:
      return ::onc::vpn::kWireGuard;
    case mojom::VpnType::kExtension:
      return ::onc::vpn::kThirdPartyVpn;
    case mojom::VpnType::kArc:
      return ::onc::vpn::kArcVpn;
  }
  NOTREACHED();
  return ::onc::vpn::kOpenVPN;
}

bool GetIsConfiguredByUser(const std::string& network_guid) {
  if (!NetworkHandler::IsInitialized())
    return false;

  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::Get()->network_metadata_store();

  if (!network_metadata_store)
    return false;

  return network_metadata_store->GetIsCreatedByUser(network_guid);
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
      return mojom::DeviceStateType::kDisabling;
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

mojom::PortalState GetMojoPortalState(
    const NetworkState::PortalState portal_state) {
  switch (portal_state) {
    case NetworkState::PortalState::kUnknown:
      return mojom::PortalState::kUnknown;
    case NetworkState::PortalState::kOnline:
      return mojom::PortalState::kOnline;
    case NetworkState::PortalState::kPortalSuspected:
      return mojom::PortalState::kPortalSuspected;
    case NetworkState::PortalState::kPortal:
      return mojom::PortalState::kPortal;
    case NetworkState::PortalState::kProxyAuthRequired:
      return mojom::PortalState::kProxyAuthRequired;
    case NetworkState::PortalState::kNoInternet:
      return mojom::PortalState::kNoInternet;
  }
  NOTREACHED();
  return mojom::PortalState::kUnknown;
}

absl::optional<GURL> GetPortalProbeUrl(const NetworkState* network) {
  switch (network->GetPortalState()) {
    case NetworkState::PortalState::kUnknown:
      [[fallthrough]];
    case NetworkState::PortalState::kOnline:
      return absl::nullopt;
    case NetworkState::PortalState::kPortalSuspected:
      [[fallthrough]];
    case NetworkState::PortalState::kPortal:
      [[fallthrough]];
    case NetworkState::PortalState::kProxyAuthRequired: {
      const GURL& probe_url = network->probe_url();
      if (probe_url.is_valid())
        return probe_url;
      else
        return GURL(captive_portal::CaptivePortalDetector::kDefaultURL);
    }
    case NetworkState::PortalState::kNoInternet:
      return absl::nullopt;
  }
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

bool IsVpnProhibited() {
  bool vpn_prohibited = false;
  if (NetworkHandler::IsInitialized()) {
    std::vector<std::string> prohibited_technologies =
        NetworkHandler::Get()
            ->prohibited_technologies_handler()
            ->GetCurrentlyProhibitedTechnologies();
    vpn_prohibited = base::Contains(prohibited_technologies, shill::kTypeVPN);
  }
  return vpn_prohibited;
}

mojom::DeviceStatePropertiesPtr GetVpnState() {
  auto result = mojom::DeviceStateProperties::New();
  result->type = mojom::NetworkType::kVPN;

  result->device_state = IsVpnProhibited() ? mojom::DeviceStateType::kProhibited
                                           : mojom::DeviceStateType::kEnabled;
  return result;
}

mojom::NetworkStatePropertiesPtr NetworkStateToMojo(
    NetworkStateHandler* network_state_handler,
    CellularESimProfileHandler* cellular_esim_profile_handler,
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
  result->connect_requested = network->connect_requested();
  bool technology_enabled = network->Matches(NetworkTypePattern::VPN()) ||
                            network_state_handler->IsTechnologyEnabled(
                                NetworkTypePattern::Primitive(network->type()));
  result->connection_state = GetConnectionState(network, technology_enabled);
  if (!network->GetError().empty())
    result->error_state = network->GetError();
  result->guid = network->guid();
  result->name =
      network_name_util::GetNetworkName(cellular_esim_profile_handler, network);
  result->portal_state = GetMojoPortalState(network->GetPortalState());
  result->portal_probe_url = GetPortalProbeUrl(network);
  result->priority = network->priority();
  result->prohibited_by_policy = network->blocked_by_policy();
  result->source = GetMojoOncSource(network);
  result->proxy_mode =
      NetworkHandler::HasUiProxyConfigService()
          ? mojom::ProxyMode(
                NetworkHandler::GetUiProxyConfigService()->ProxyModeForNetwork(
                    network))
          : mojom::ProxyMode::kDirect;
  result->dns_queries_monitored =
      NetworkHandler::IsInitialized() &&
              NetworkHandler::Get()->network_metadata_store()
          ? NetworkHandler::Get()
                ->network_metadata_store()
                ->secure_dns_templates_with_identifiers_active()
          : false;

  switch (type) {
    case mojom::NetworkType::kCellular: {
      auto cellular = mojom::CellularStateProperties::New();
      cellular->iccid = network->iccid();
      cellular->eid = network->eid();
      cellular->activation_state = network->GetMojoActivationState();
      cellular->network_technology = ShillToOnc(network->network_technology(),
                                                onc::kNetworkTechnologyTable);
      cellular->roaming = network->IndicateRoaming();
      cellular->signal_strength = network->signal_strength();

      const DeviceState* cellular_device =
          network_state_handler->GetDeviceState(network->device_path());
      bool sim_is_primary =
          cellular_device && IsSimPrimary(network->iccid(), cellular_device);
      cellular->sim_lock_enabled =
          sim_is_primary && cellular_device->sim_lock_enabled();
      cellular->sim_locked = sim_is_primary && cellular_device->IsSimLocked();
      if (sim_is_primary)
        cellular->sim_lock_type = cellular_device->sim_lock_type();
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
      wifi->hidden_ssid = network->hidden_ssid();
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

std::vector<mojom::SIMInfoPtr> CellularSIMInfosToMojo(
    const DeviceState* device) {
  std::vector<mojom::SIMInfoPtr> sim_info_mojos;
  for (const auto& sim_slot : GetSimSlotInfosWithUpdatedEid(device)) {
    auto sim_info_mojo = mojom::SIMInfo::New();
    sim_info_mojo->slot_id = sim_slot.slot_id;
    sim_info_mojo->iccid = sim_slot.iccid;
    sim_info_mojo->eid = sim_slot.eid;
    sim_info_mojo->is_primary = sim_slot.primary;
    sim_info_mojos.push_back(std::move(sim_info_mojo));
  }
  return sim_info_mojos;
}

bool IsCellularConnecting(NetworkStateHandler* network_state_handler) {
  NetworkStateHandler::NetworkStateList cellular_networks;
  network_state_handler->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &cellular_networks);
  return base::ranges::any_of(cellular_networks,
                              &NetworkState::IsConnectingState);
}

mojom::InhibitReason GetInhibitReason(
    NetworkStateHandler* network_state_handler,
    CellularInhibitor* cellular_inhibitor) {
  if (!cellular_inhibitor)
    return mojom::InhibitReason::kNotInhibited;

  absl::optional<CellularInhibitor::InhibitReason> inhibit_reason =
      cellular_inhibitor->GetInhibitReason();
  if (!inhibit_reason) {
    // For devices with EUICC, the UI should be inhibited when a cellular
    // network connection is in progress to prevent additional requests. This is
    // due to complexity in switching slots.
    if (!chromeos::HermesManagerClient::Get()->GetAvailableEuiccs().empty() &&
        IsCellularConnecting(network_state_handler)) {
      return mojom::InhibitReason::kConnectingToProfile;
    }

    return mojom::InhibitReason::kNotInhibited;
  }

  switch (*inhibit_reason) {
    case CellularInhibitor::InhibitReason::kInstallingProfile:
      return mojom::InhibitReason::kInstallingProfile;
    case CellularInhibitor::InhibitReason::kRenamingProfile:
      return mojom::InhibitReason::kRenamingProfile;
    case CellularInhibitor::InhibitReason::kRemovingProfile:
      return mojom::InhibitReason::kRemovingProfile;
    case CellularInhibitor::InhibitReason::kConnectingToProfile:
      return mojom::InhibitReason::kConnectingToProfile;
    case CellularInhibitor::InhibitReason::kRefreshingProfileList:
      return mojom::InhibitReason::kRefreshingProfileList;
    case CellularInhibitor::InhibitReason::kResettingEuiccMemory:
      return mojom::InhibitReason::kResettingEuiccMemory;
    case CellularInhibitor::InhibitReason::kDisablingProfile:
      return mojom::InhibitReason::kDisablingProfile;
  }
}

mojom::DeviceStatePropertiesPtr DeviceStateToMojo(
    const DeviceState* device,
    NetworkStateHandler* network_state_handler,
    CellularInhibitor* cellular_inhibitor,
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
  if (type == mojom::NetworkType::kCellular) {
    result->sim_infos = CellularSIMInfosToMojo(device);
    result->inhibit_reason =
        GetInhibitReason(network_state_handler, cellular_inhibitor);
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

absl::optional<std::string> GetString(const base::Value* dict,
                                      const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_string()) {
    NET_LOG(ERROR) << "Expected string, found: " << *v;
    return absl::nullopt;
  }
  return v ? absl::make_optional<std::string>(v->GetString()) : absl::nullopt;
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

bool GetBoolean(const base::Value* dict,
                const char* key,
                bool value_if_key_missing_from_dict = false) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_bool()) {
    NET_LOG(ERROR) << "Expected bool, found: " << *v;
    return false;
  }
  return v ? v->GetBool() : value_if_key_missing_from_dict;
}

int32_t GetInt32(const base::Value* dict, const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (v && !v->is_int()) {
    NET_LOG(ERROR) << "Expected int, found: " << *v;
    return 0;
  }
  return v ? v->GetInt() : 0;
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

absl::optional<std::vector<std::string>> GetStringList(const base::Value* dict,
                                                       const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v)
    return absl::nullopt;
  if (!v->is_list()) {
    NET_LOG(ERROR) << "Expected list, found: " << *v;
    return absl::nullopt;
  }
  std::vector<std::string> result;
  for (const base::Value& e : v->GetList())
    result.push_back(e.GetString());
  return result;
}

std::vector<std::string> GetRequiredStringList(const base::Value* dict,
                                               const char* key) {
  const base::Value* v = dict->FindKey(key);
  if (!v) {
    NOTREACHED() << "Required key missing: " << key;
    return {};
  }
  if (!v->is_list()) {
    NET_LOG(ERROR) << "Expected list, found: " << *v;
    return {};
  }
  std::vector<std::string> result;
  result.reserve(v->GetList().size());
  for (const base::Value& e : v->GetList()) {
    if (!e.is_string()) {
      NET_LOG(ERROR) << "Expected string, found: " << e;
      break;
    }
    result.push_back(e.GetString());
  }
  return result;
}

void SetString(const char* key,
               const absl::optional<std::string>& property,
               base::Value* dict) {
  if (!property)
    return;
  dict->SetStringKey(key, *property);
}

void SetString(const char* key,
               const absl::optional<std::string>& property,
               base::Value::Dict* dict) {
  if (!property)
    return;
  dict->Set(key, *property);
}

void SetStringIfNotEmpty(const char* key,
                         const absl::optional<std::string>& property,
                         base::Value::Dict* dict) {
  if (!property || property->empty())
    return;
  dict->Set(key, *property);
}

void SetStringList(const char* key,
                   const absl::optional<std::vector<std::string>>& property,
                   base::Value* dict) {
  if (!property)
    return;
  base::Value list(base::Value::Type::LIST);
  for (const std::string& s : *property)
    list.Append(base::Value(s));
  dict->SetKey(key, std::move(list));
}

void SetStringList(const char* key,
                   const absl::optional<std::vector<std::string>>& property,
                   base::Value::Dict* dict) {
  if (!property)
    return;
  base::Value::List list;
  for (const std::string& s : *property)
    list.Append(s);
  dict->Set(key, std::move(list));
}

void SetSubjectAltNameMatch(
    const char* key,
    const std::vector<mojom::SubjectAltNamePtr>* property,
    base::Value* dict) {
  base::Value subject_alt_name_list(base::Value::Type::LIST);
  for (const auto& ptr : *property) {
    std::string type;
    switch (ptr->type) {
      case mojom::SubjectAltName::Type::kEmail:
        type = ::onc::eap_subject_alternative_name_match::kEMAIL;
        break;
      case mojom::SubjectAltName::Type::kDns:
        type = ::onc::eap_subject_alternative_name_match::kDNS;
        break;
      case mojom::SubjectAltName::Type::kUri:
        type = ::onc::eap_subject_alternative_name_match::kURI;
        break;
    }
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey(::onc::eap_subject_alternative_name_match::kType, type);
    entry.SetStringKey(::onc::eap_subject_alternative_name_match::kValue,
                       ptr->value);
    subject_alt_name_list.Append(std::move(entry));
  }
  dict->SetKey(key, std::move(subject_alt_name_list));
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

  absl::optional<std::string> effective =
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

mojom::SubjectAltNamePtr GetSubjectAltName(const base::Value* dict) {
  auto san = mojom::SubjectAltName::New();
  san->value = GetRequiredString(
      dict, ::onc::eap_subject_alternative_name_match::kValue);
  std::string type =
      GetRequiredString(dict, ::onc::eap_subject_alternative_name_match::kType);
  if (type == ::onc::eap_subject_alternative_name_match::kEMAIL) {
    san->type = mojom::SubjectAltName::Type::kEmail;
  } else if (type == ::onc::eap_subject_alternative_name_match::kDNS) {
    san->type = mojom::SubjectAltName::Type::kDns;
  } else if (type == ::onc::eap_subject_alternative_name_match::kURI) {
    san->type = mojom::SubjectAltName::Type::kUri;
  } else {
    NET_LOG(ERROR) << "Unknown subject alternative name type " << type;
    return nullptr;
  }
  return san;
}

mojom::ManagedSubjectAltNameMatchListPtr GetManagedSubjectAltNameMatchList(
    const base::Value* dict,
    const char* key) {
  auto result = mojom::ManagedSubjectAltNameMatchList::New();
  const base::Value* value = dict->FindKey(key);
  if (!value)
    return result;

  if (value->is_list()) {
    std::vector<mojom::SubjectAltNamePtr> active;
    for (const base::Value& e : value->GetList())
      active.push_back(GetSubjectAltName(&e));
    result->active_value = std::move(active);
    return result;
  }
  if (value->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(value);
    if (!managed_dict.active_value.is_list()) {
      NET_LOG(ERROR) << "No active or effective value for WireGuardPeerList";
      return result;
    }
    for (const base::Value& e : managed_dict.active_value.GetList())
      result->active_value.push_back(GetSubjectAltName(&e));
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none()) {
      result->policy_value = std::vector<mojom::SubjectAltNamePtr>();
      for (const base::Value& e : managed_dict.policy_value.GetList())
        result->policy_value.push_back(GetSubjectAltName(&e));
    }
    return result;
  }
  NET_LOG(ERROR) << "Expected list or dictionary, found: " << *value;
  return result;
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
  auto type = GetString(dict, ::onc::ipconfig::kType);
  if (!type || type->empty()) {
    // Shill may omit the IP Config type for VPNs. The type should be IPv4.
    ip_config->type = mojom::IPConfigType::kIPv4;
  } else {
    ip_config->type = OncIPConfigTypeToMojo(*type);
  }
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
  // The IPConfig type is not actually mutable, so we convert from an optional
  // managed string to a required unmanaged type enum.
  mojom::ManagedStringPtr managed_type =
      GetManagedString(dict, ::onc::ipconfig::kType);
  mojom::IPConfigType type =
      managed_type ? OncIPConfigTypeToMojo(managed_type->active_value)
                   : mojom::IPConfigType::kIPv4;
  ip_config->type = type;
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
                      base::Value::Dict* dict) {
  if (location.is_null())
    return;
  base::Value::Dict location_dict;
  location_dict.Set(::onc::proxy::kHost, location->host);
  location_dict.Set(::onc::proxy::kPort, location->port);
  dict->Set(key, std::move(location_dict));
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

mojom::ApnState OncApnStateTypeToMojo(const std::string* state) {
  DCHECK(ash::features::IsApnRevampEnabled());
  // State can be empty, because database/modem APNs won't have a state.
  if (!state || state->empty() || *state == ::onc::cellular_apn::kStateEnabled)
    return mojom::ApnState::kEnabled;
  if (*state == ::onc::cellular_apn::kStateDisabled)
    return mojom::ApnState::kDisabled;

  NOTREACHED() << "Unexpected ONC APN State type: " << state;
  return mojom::ApnState::kEnabled;
}

std::string MojoApnStateTypeToOnc(mojom::ApnState state) {
  DCHECK(ash::features::IsApnRevampEnabled());
  switch (state) {
    case mojom::ApnState::kDisabled:
      return ::onc::cellular_apn::kStateDisabled;
    case mojom::ApnState::kEnabled:
      return ::onc::cellular_apn::kStateEnabled;
  }
  NOTREACHED() << "Unexpected mojo ApnState type: " << state;
  return ::onc::cellular_apn::kStateEnabled;
}

mojom::ApnAuthenticationType OncApnAuthenticationTypeToMojo(
    const std::string& authentication_type) {
  DCHECK(ash::features::IsApnRevampEnabled());
  if (authentication_type.empty() ||
      authentication_type ==
          ::onc::cellular_apn::kAuthenticationTypeAutomatic) {
    return mojom::ApnAuthenticationType::kAutomatic;
  }
  if (authentication_type == ::onc::cellular_apn::kAuthenticationTypePap)
    return mojom::ApnAuthenticationType::kPap;
  if (authentication_type == ::onc::cellular_apn::kAuthenticationTypeChap)
    return mojom::ApnAuthenticationType::kChap;

  NOTREACHED() << "Unexpected ONC APN Authentication type: "
               << authentication_type;
  return mojom::ApnAuthenticationType::kAutomatic;
}

std::string MojoApnAuthenticationTypeToOnc(
    mojom::ApnAuthenticationType authentication_type) {
  DCHECK(ash::features::IsApnRevampEnabled());
  switch (authentication_type) {
    case mojom::ApnAuthenticationType::kAutomatic:
      return ::onc::cellular_apn::kAuthenticationTypeAutomatic;
    case mojom::ApnAuthenticationType::kPap:
      return ::onc::cellular_apn::kAuthenticationTypePap;
    case mojom::ApnAuthenticationType::kChap:
      return ::onc::cellular_apn::kAuthenticationTypeChap;
  }
  NOTREACHED() << "Unexpected mojo AuthenticationType type: "
               << authentication_type;
  return ::onc::cellular_apn::kAuthenticationTypeAutomatic;
}

mojom::ApnIpType OncApnIpTypeToMojo(const std::string& ip_type) {
  DCHECK(ash::features::IsApnRevampEnabled());
  if (ip_type.empty() || ip_type == ::onc::cellular_apn::kIpTypeAutomatic)
    return mojom::ApnIpType::kAutomatic;
  if (ip_type == ::onc::cellular_apn::kIpTypeIpv4)
    return mojom::ApnIpType::kIpv4;
  if (ip_type == ::onc::cellular_apn::kIpTypeIpv6)
    return mojom::ApnIpType::kIpv6;
  if (ip_type == ::onc::cellular_apn::kIpTypeIpv4Ipv6)
    return mojom::ApnIpType::kIpv4Ipv6;

  NOTREACHED() << "Unexpected ONC APN IP type: " << ip_type;
  return mojom::ApnIpType::kAutomatic;
}

std::string MojoApnIpTypeToOnc(mojom::ApnIpType ip_type) {
  DCHECK(ash::features::IsApnRevampEnabled());
  switch (ip_type) {
    case mojom::ApnIpType::kAutomatic:
      return ::onc::cellular_apn::kIpTypeAutomatic;
    case mojom::ApnIpType::kIpv4:
      return ::onc::cellular_apn::kIpTypeIpv4;
    case mojom::ApnIpType::kIpv6:
      return ::onc::cellular_apn::kIpTypeIpv6;
    case mojom::ApnIpType::kIpv4Ipv6:
      return ::onc::cellular_apn::kIpTypeIpv4Ipv6;
  }
  NOTREACHED() << "Unexpected mojo ApnIpType type: " << ip_type;
  return ::onc::cellular_apn::kIpTypeAutomatic;
}

std::vector<mojom::ApnType> OncApnTypesToMojo(
    const std::vector<std::string>& apn_types) {
  DCHECK(ash::features::IsApnRevampEnabled());
  DCHECK(!apn_types.empty());
  std::vector<mojom::ApnType> apn_types_result;
  apn_types_result.reserve(apn_types.size());
  for (const std::string& apn_type : apn_types) {
    if (apn_type == ::onc::cellular_apn::kApnTypeDefault) {
      apn_types_result.push_back(mojom::ApnType::kDefault);
      continue;
    }
    if (apn_type == ::onc::cellular_apn::kApnTypeAttach) {
      apn_types_result.push_back(mojom::ApnType::kAttach);
      continue;
    }

    NOTREACHED() << "Unexpected ONC APN Type: " << apn_type;
  }

  return apn_types_result;
}

std::vector<std::string> MojoApnTypesToOnc(
    const std::vector<mojom::ApnType>& apn_types) {
  DCHECK(ash::features::IsApnRevampEnabled());
  DCHECK(!apn_types.empty());
  std::vector<std::string> apn_types_result;
  apn_types_result.reserve(apn_types.size());
  for (const mojom::ApnType& type : apn_types) {
    switch (type) {
      case mojom::ApnType::kDefault:
        apn_types_result.push_back(::onc::cellular_apn::kApnTypeDefault);
        continue;
      case mojom::ApnType::kAttach:
        apn_types_result.push_back(::onc::cellular_apn::kApnTypeAttach);
        continue;
    }
  }

  return apn_types_result;
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
  apn->attach = GetString(dict, ::onc::cellular_apn::kAttach);

  if (ash::features::IsApnRevampEnabled()) {
    apn->id = GetString(dict, ::onc::cellular_apn::kId);
    // TODO(b/162365553) Remove missing value checking after Shill implements
    // the interface.
    if (!GetString(dict, ::onc::cellular_apn::kAuthenticationType)) {
      apn->authentication_type = mojom::ApnAuthenticationType::kAutomatic;
    } else {
      apn->authentication_type = OncApnAuthenticationTypeToMojo(
          GetRequiredString(dict, ::onc::cellular_apn::kAuthenticationType));
    }
    // TODO(b/162365553) Remove missing value checking after Shill implements
    // the interface.
    if (!GetString(dict, ::onc::cellular_apn::kIpType)) {
      apn->ip_type = mojom::ApnIpType::kAutomatic;
    } else {
      apn->ip_type = OncApnIpTypeToMojo(
          GetRequiredString(dict, ::onc::cellular_apn::kIpType));
    }
    // TODO(b/162365553) Remove missing value checking after Shill implements
    // the interface.
    if (!GetStringList(dict, ::onc::cellular_apn::kApnTypes)) {
      apn->apn_types = {mojom::ApnType::kDefault};
    } else {
      apn->apn_types = OncApnTypesToMojo(
          GetRequiredStringList(dict, ::onc::cellular_apn::kApnTypes));
    }
  }

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
  apn->attach = GetManagedString(apn_dict, ::onc::cellular_apn::kAttach);
  return apn;
}

mojom::ManagedApnListPtr GetManagedApnList(const base::Value* value) {
  if (!value)
    return nullptr;
  if (value->is_list()) {
    auto result = mojom::ManagedApnList::New();
    std::vector<mojom::ApnPropertiesPtr> active;
    for (const base::Value& e : value->GetList())
      active.push_back(GetApnProperties(&e));
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
  eap->client_cert_provisioning_profile_id = GetManagedString(
      eap_dict, ::onc::client_cert::kClientCertProvisioningProfileId);
  eap->client_cert_ref =
      GetManagedString(eap_dict, ::onc::client_cert::kClientCertRef);
  eap->client_cert_type =
      GetManagedString(eap_dict, ::onc::client_cert::kClientCertType);
  eap->domain_suffix_match =
      GetManagedStringList(eap_dict, ::onc::eap::kDomainSuffixMatch);
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
  eap->subject_alt_name_match = GetManagedSubjectAltNameMatchList(
      eap_dict, ::onc::eap::kSubjectAlternativeNameMatch);
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
  ipsec->client_cert_provisioning_profile_id = GetManagedString(
      ipsec_dict, ::onc::client_cert::kClientCertProvisioningProfileId);
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
  ipsec->local_identity =
      GetManagedString(ipsec_dict, ::onc::ipsec::kLocalIdentity);
  ipsec->remote_identity =
      GetManagedString(ipsec_dict, ::onc::ipsec::kRemoteIdentity);
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
  openvpn->client_cert_provisioning_profile_id = GetManagedString(
      openvpn_dict, ::onc::client_cert::kClientCertProvisioningProfileId);
  openvpn->client_cert_ref =
      GetManagedString(openvpn_dict, ::onc::client_cert::kClientCertRef);
  openvpn->client_cert_type =
      GetManagedString(openvpn_dict, ::onc::client_cert::kClientCertType);
  openvpn->compression_algorithm =
      GetManagedString(openvpn_dict, ::onc::openvpn::kCompressionAlgorithm);
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

mojom::WireGuardPeerPropertiesPtr GetWireGuardPeerProperties(
    const base::Value* dict) {
  auto peer = mojom::WireGuardPeerProperties::New();
  peer->public_key = GetRequiredString(dict, ::onc::wireguard::kPublicKey);
  peer->preshared_key = GetString(dict, ::onc::wireguard::kPresharedKey);
  peer->allowed_ips = GetString(dict, ::onc::wireguard::kAllowedIPs);
  peer->endpoint = GetString(dict, ::onc::wireguard::kEndpoint);
  peer->persistent_keepalive_interval =
      GetInt32(dict, ::onc::wireguard::kPersistentKeepalive);
  return peer;
}

mojom::ManagedWireGuardPeerListPtr GetManagedWireGuardPeerList(
    const base::Value* dict,
    const char* key) {
  auto result = mojom::ManagedWireGuardPeerList::New();
  const base::Value* value = dict->FindKey(key);
  if (!value)
    return result;
  if (value->is_list()) {
    std::vector<mojom::WireGuardPeerPropertiesPtr> active;
    for (const base::Value& e : value->GetList())
      active.push_back(GetWireGuardPeerProperties(&e));
    result->active_value = std::move(active);
    return result;
  }
  if (value->is_dict()) {
    ManagedDictionary managed_dict = GetManagedDictionary(value);
    if (!managed_dict.active_value.is_list()) {
      NET_LOG(ERROR) << "No active or effective value for WireGuardPeerList";
      return result;
    }
    for (const base::Value& e : managed_dict.active_value.GetList())
      result->active_value.push_back(GetWireGuardPeerProperties(&e));
    result->policy_source = managed_dict.policy_source;
    if (!managed_dict.policy_value.is_none()) {
      result->policy_value = std::vector<mojom::WireGuardPeerPropertiesPtr>();
      for (const base::Value& e : managed_dict.policy_value.GetList())
        result->policy_value->push_back(GetWireGuardPeerProperties(&e));
    }
    return result;
  }
  NET_LOG(ERROR) << "Expected list or dictionary, found: " << *value;
  return result;
}

mojom::ManagedWireGuardPropertiesPtr GetManagedWireGuardProperties(
    const base::Value* dict,
    const char* key) {
  auto wg = mojom::ManagedWireGuardProperties::New();
  const base::Value* wg_dict = dict->FindKey(key);
  if (!wg_dict) {
    NET_LOG(ERROR) << "Missing WireGuard properties element";
    return wg;
  }
  if (!wg_dict->is_dict()) {
    NET_LOG(ERROR) << "Expected dictionary, found: " << *wg_dict;
    return wg;
  }
  wg->private_key = GetManagedString(wg_dict, ::onc::wireguard::kPrivateKey);
  wg->public_key = GetManagedString(wg_dict, ::onc::wireguard::kPublicKey);
  wg->peers = GetManagedWireGuardPeerList(wg_dict, ::onc::wireguard::kPeers);
  return wg;
}

mojom::ManagedPropertiesPtr ManagedPropertiesToMojo(
    NetworkStateHandler* network_state_handler,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state,
    const std::vector<mojom::VpnProviderPtr>& vpn_providers,
    const base::Value* properties) {
  DCHECK(network_state);
  DCHECK(properties);
  absl::optional<std::string> onc_type =
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
  absl::optional<std::string> guid =
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
  result->portal_state = GetMojoPortalState(network_state->GetPortalState());
  const base::Value* saved_ip_config =
      GetDictionary(properties, ::onc::network_config::kSavedIPConfig);
  if (saved_ip_config)
    result->saved_ip_config = GetIPConfig(saved_ip_config);

  // Managed properties
  result->ip_address_config_type = GetRequiredManagedString(
      properties, ::onc::network_config::kIPAddressConfigType);
  result->metered =
      GetManagedBoolean(properties, ::onc::network_config::kMetered);
  result->name = GetManagedString(properties, ::onc::network_config::kName);
  if (result->name->policy_source == mojom::PolicySource::kNone) {
    absl::optional<std::string> profile_name =
        network_name_util::GetESimProfileName(cellular_esim_profile_handler,
                                              network_state);
    if (profile_name)
      result->name->active_value = *profile_name;
  }

  result->name_servers_config_type = GetRequiredManagedString(
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
      cellular->selected_apn =
          GetManagedApnProperties(cellular_dict, ::onc::cellular::kAPN);
      cellular->apn_list =
          GetManagedApnList(cellular_dict->FindKey(::onc::cellular::kAPNList));
      cellular->allow_roaming =
          GetManagedBoolean(cellular_dict, ::onc::cellular::kAllowRoaming);
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
      cellular->eid = GetString(cellular_dict, ::onc::cellular::kEID);
      cellular->iccid = GetString(cellular_dict, ::onc::cellular::kICCID);
      cellular->imei = GetString(cellular_dict, ::onc::cellular::kIMEI);
      const base::Value* apn_dict =
          GetDictionary(cellular_dict, ::onc::cellular::kLastGoodAPN);
      if (apn_dict) {
        cellular->last_good_apn = GetApnProperties(apn_dict);
        if (ash::features::IsApnRevampEnabled()) {
          const absl::optional<std::string> connection_state =
              GetString(properties, ::onc::network_config::kConnectionState);

          // The connected_apn will only be set when the network is connected,
          // and will indicate which APN was used to establish the data
          // connection. The last_good_apn property is set by Shill, and can be
          // present when the network is not connected.
          if (connection_state &&
              *connection_state == ::onc::connection_state::kConnected) {
            cellular->connected_apn = GetApnProperties(apn_dict);
          }
        }
      }
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

      const DeviceState* cellular_device =
          network_state_handler->GetDeviceState(network_state->device_path());

      // The cellular device only tracks whether the active SIM is locked. To
      // determine whether |network_state| is locked, we check that the SIM is
      // active by comparing the ICCID to the device's ICCID, then we check that
      // the device is in a locked state.
      cellular->sim_locked = cellular_device &&
                             cellular_device->iccid() == cellular->iccid &&
                             cellular_device->IsSimLocked();

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
        case mojom::VpnType::kIKEv2:
          vpn->ip_sec = GetManagedIPSecProperties(vpn_dict, ::onc::vpn::kIPsec);
          break;
        case mojom::VpnType::kL2TPIPsec:
          vpn->ip_sec = GetManagedIPSecProperties(vpn_dict, ::onc::vpn::kIPsec);
          vpn->l2tp = GetManagedL2TPProperties(vpn_dict, ::onc::vpn::kL2TP);
          break;
        case mojom::VpnType::kOpenVPN:
          vpn->open_vpn =
              GetManagedOpenVPNProperties(vpn_dict, ::onc::vpn::kOpenVPN);
          break;
        case mojom::VpnType::kWireGuard:
          vpn->wireguard =
              GetManagedWireGuardProperties(vpn_dict, ::onc::vpn::kWireGuard);
          break;
        case mojom::VpnType::kExtension:
        case mojom::VpnType::kArc:
          const base::Value* third_party_dict =
              vpn_dict->FindKey(::onc::vpn::kThirdPartyVpn);
          if (third_party_dict) {
            vpn->provider_id = GetManagedString(
                third_party_dict, ::onc::third_party_vpn::kExtensionID);
            absl::optional<std::string> provider_name = GetString(
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
      wifi->hex_ssid = GetManagedString(wifi_dict, ::onc::wifi::kHexSSID);
      wifi->hidden_ssid =
          GetManagedBoolean(wifi_dict, ::onc::wifi::kHiddenSSID);
      wifi->passphrase = GetManagedString(wifi_dict, ::onc::wifi::kPassphrase);
      wifi->ssid = GetRequiredManagedString(wifi_dict, ::onc::wifi::kSSID);
      CHECK(wifi->ssid);
      wifi->signal_strength = GetInt32(wifi_dict, ::onc::wifi::kSignalStrength);
      wifi->is_syncable = sync_wifi::IsEligibleForSync(
          result->guid, result->connectable,
          wifi->hidden_ssid ? wifi->hidden_ssid->active_value : false,
          wifi->security, result->source,
          /*log_result=*/false);
      wifi->is_configured_by_active_user = GetIsConfiguredByUser(result->guid);

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

  // Traffic Counter Properties
  auto traffic_counter_properties = mojom::TrafficCounterProperties::New();
  const base::Value* last_reset_time =
      properties->FindKey(::onc::network_config::kTrafficCounterResetTime);
  if (last_reset_time && last_reset_time->is_double()) {
    traffic_counter_properties->last_reset_time =
        base::Time::FromDeltaSinceWindowsEpoch(
            base::Milliseconds(last_reset_time->GetDouble()));
    traffic_counter_properties->friendly_date =
        base::UTF16ToUTF8(base::TimeFormatFriendlyDate(
            traffic_counter_properties->last_reset_time.value()));
  } else {
    traffic_counter_properties->last_reset_time = absl::nullopt;
    traffic_counter_properties->friendly_date = absl::nullopt;
  }

  const base::Value* auto_reset =
      NetworkHandler::IsInitialized()
          ? NetworkHandler::Get()
                ->network_metadata_store()
                ->GetEnableTrafficCountersAutoReset(result->guid)
          : nullptr;
  traffic_counter_properties->auto_reset =
      auto_reset && auto_reset->is_bool() ? auto_reset->GetBool() : false;
  const base::Value* user_specified_reset_day =
      NetworkHandler::IsInitialized()
          ? NetworkHandler::Get()
                ->network_metadata_store()
                ->GetDayOfTrafficCountersAutoReset(result->guid)
          : nullptr;
  traffic_counter_properties->user_specified_reset_day =
      user_specified_reset_day && user_specified_reset_day->is_int()
          ? user_specified_reset_day->GetInt()
          : kDefaultResetDay;
  result->traffic_counter_properties = std::move(traffic_counter_properties);

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
  SetStringList(::onc::eap::kDomainSuffixMatch, eap.domain_suffix_match,
                &eap_dict);
  SetString(::onc::eap::kIdentity, eap.identity, &eap_dict);
  SetString(::onc::eap::kInner, eap.inner, &eap_dict);
  SetString(::onc::eap::kOuter, eap.outer, &eap_dict);
  SetString(::onc::eap::kPassword, eap.password, &eap_dict);
  eap_dict.SetBoolKey(::onc::eap::kSaveCredentials, eap.save_credentials);
  SetStringList(::onc::eap::kServerCAPEMs, eap.server_ca_pems, &eap_dict);
  SetSubjectAltNameMatch(::onc::eap::kSubjectAlternativeNameMatch,
                         &eap.subject_alt_name_match, &eap_dict);
  SetString(::onc::eap::kSubjectMatch, eap.subject_match, &eap_dict);
  eap_dict.SetBoolKey(::onc::eap::kUseSystemCAs, eap.use_system_cas);

  return eap_dict;
}

base::Value::Dict MojoApnToOnc(const mojom::ApnProperties& apn_props) {
  base::Value::Dict apn;
  apn.Set(::onc::cellular_apn::kAccessPointName, apn_props.access_point_name);
  SetString(::onc::cellular_apn::kAuthentication, apn_props.authentication,
            &apn);
  SetString(::onc::cellular_apn::kLanguage, apn_props.language, &apn);
  SetString(::onc::cellular_apn::kLocalizedName, apn_props.localized_name,
            &apn);
  SetString(::onc::cellular_apn::kName, apn_props.name, &apn);
  SetString(::onc::cellular_apn::kPassword, apn_props.password, &apn);
  SetString(::onc::cellular_apn::kUsername, apn_props.username, &apn);
  SetString(::onc::cellular_apn::kAttach, apn_props.attach, &apn);
  if (ash::features::IsApnRevampEnabled()) {
    SetString(::onc::cellular_apn::kId, apn_props.id, &apn);
    apn.Set(::onc::cellular_apn::kState,
            MojoApnStateTypeToOnc(apn_props.state));
    apn.Set(::onc::cellular_apn::kAuthenticationType,
            MojoApnAuthenticationTypeToOnc(apn_props.authentication_type));
    apn.Set(::onc::cellular_apn::kIpType,
            MojoApnIpTypeToOnc(apn_props.ip_type));
    base::Value::List apn_types;
    for (const std::string& apn_type : MojoApnTypesToOnc(apn_props.apn_types))
      apn_types.Append(apn_type);
    apn.Set(::onc::cellular_apn::kApnTypes, std::move(apn_types));
  }
  return apn;
}

absl::optional<base::Value::Dict> GetOncFromConfigProperties(
    const mojom::ConfigProperties* properties,
    absl::optional<std::string> guid) {
  base::Value::Dict onc;

  if (properties->guid && !properties->guid->empty()) {
    if (guid && *guid != *properties->guid) {
      NET_LOG(ERROR) << "GUID does not match: " << *guid
                     << " != " << *properties->guid;
      return absl::nullopt;
    }
    SetString(::onc::network_config::kGUID, *properties->guid, &onc);
  }

  // Process |properties->network_type| and set |type|. Configurations have only
  // one type dictionary.
  mojom::NetworkType type = mojom::NetworkType::kAll;  // Invalid type
  base::Value::Dict type_dict;

  if (properties->type_config->is_cellular()) {
    type = mojom::NetworkType::kCellular;
    const mojom::CellularConfigProperties& cellular =
        *properties->type_config->get_cellular();
    if (cellular.apn) {
      type_dict.Set(::onc::cellular::kAPN, MojoApnToOnc(*cellular.apn));
    }
    if (cellular.roaming) {
      type_dict.Set(::onc::cellular::kAllowRoaming,
                    base::Value(cellular.roaming->allow_roaming));
    }
  } else if (properties->type_config->is_ethernet()) {
    type = mojom::NetworkType::kEthernet;
    const mojom::EthernetConfigProperties& ethernet =
        *properties->type_config->get_ethernet();
    SetString(::onc::ethernet::kAuthentication, ethernet.authentication,
              &type_dict);
    if (ethernet.eap) {
      type_dict.Set(::onc::ethernet::kEAP,
                    GetEAPProperties(*ethernet.eap.get()));
    }
  } else if (properties->type_config->is_vpn()) {
    type = mojom::NetworkType::kVPN;
    const mojom::VPNConfigProperties& vpn = *properties->type_config->get_vpn();
    SetString(::onc::vpn::kHost, vpn.host, &type_dict);
    if (vpn.ip_sec) {
      const mojom::IPSecConfigProperties& ip_sec = *vpn.ip_sec;
      base::Value::Dict ip_sec_dict;
      SetString(::onc::ipsec::kAuthenticationType, ip_sec.authentication_type,
                &ip_sec_dict);
      SetString(::onc::client_cert::kClientCertPKCS11Id,
                ip_sec.client_cert_pkcs11_id, &ip_sec_dict);
      SetString(::onc::client_cert::kClientCertType, ip_sec.client_cert_type,
                &ip_sec_dict);
      SetString(::onc::ipsec::kGroup, ip_sec.group, &ip_sec_dict);
      ip_sec_dict.Set(::onc::ipsec::kIKEVersion, ip_sec.ike_version);
      SetString(::onc::ipsec::kPSK, ip_sec.psk, &ip_sec_dict);
      ip_sec_dict.Set(::onc::l2tp::kSaveCredentials, ip_sec.save_credentials);
      SetStringList(::onc::ipsec::kServerCAPEMs, ip_sec.server_ca_pems,
                    &ip_sec_dict);
      SetStringList(::onc::ipsec::kServerCARefs, ip_sec.server_ca_refs,
                    &ip_sec_dict);
      SetString(::onc::ipsec::kLocalIdentity, ip_sec.local_identity,
                &ip_sec_dict);
      SetString(::onc::ipsec::kRemoteIdentity, ip_sec.remote_identity,
                &ip_sec_dict);
      if (ip_sec.eap) {
        ip_sec_dict.Set(::onc::ipsec::kEAP,
                        GetEAPProperties(*ip_sec.eap.get()));
      }
      type_dict.Set(::onc::vpn::kIPsec, std::move(ip_sec_dict));
    }
    if (vpn.l2tp) {
      const mojom::L2TPConfigProperties& l2tp = *vpn.l2tp;
      base::Value::Dict l2tp_dict;
      l2tp_dict.Set(::onc::l2tp::kLcpEchoDisabled, l2tp.lcp_echo_disabled);
      SetString(::onc::l2tp::kPassword, l2tp.password, &l2tp_dict);
      l2tp_dict.Set(::onc::l2tp::kSaveCredentials, l2tp.save_credentials);
      SetString(::onc::l2tp::kUsername, l2tp.username, &l2tp_dict);
      type_dict.Set(::onc::vpn::kL2TP, std::move(l2tp_dict));
    }
    if (vpn.open_vpn) {
      const mojom::OpenVPNConfigProperties& open_vpn = *vpn.open_vpn;
      base::Value::Dict open_vpn_dict;
      SetString(::onc::client_cert::kClientCertPKCS11Id,
                open_vpn.client_cert_pkcs11_id, &open_vpn_dict);
      SetString(::onc::client_cert::kClientCertType, open_vpn.client_cert_type,
                &open_vpn_dict);
      SetStringList(::onc::openvpn::kExtraHosts, open_vpn.extra_hosts,
                    &open_vpn_dict);
      SetString(::onc::openvpn::kOTP, open_vpn.otp, &open_vpn_dict);
      SetString(::onc::openvpn::kPassword, open_vpn.password, &open_vpn_dict);
      open_vpn_dict.Set(::onc::l2tp::kSaveCredentials,
                        open_vpn.save_credentials);
      SetStringList(::onc::openvpn::kServerCAPEMs, open_vpn.server_ca_pems,
                    &open_vpn_dict);
      SetStringList(::onc::openvpn::kServerCARefs, open_vpn.server_ca_refs,
                    &open_vpn_dict);
      SetString(::onc::vpn::kUsername, open_vpn.username, &open_vpn_dict);
      SetString(::onc::openvpn::kUserAuthenticationType,
                open_vpn.user_authentication_type, &open_vpn_dict);
      type_dict.Set(::onc::vpn::kOpenVPN, std::move(open_vpn_dict));
    }
    if (vpn.wireguard) {
      const mojom::WireGuardConfigProperties& wireguard = *vpn.wireguard;
      base::Value::Dict wireguard_dict;
      SetString(::onc::wireguard::kPrivateKey, wireguard.private_key,
                &wireguard_dict);

      base::Value::List peer_list;
      if (wireguard.peers) {
        for (auto const& peer : *wireguard.peers) {
          base::Value::Dict peer_dict;
          peer_dict.Set(::onc::wireguard::kPublicKey, peer->public_key);
          SetString(::onc::wireguard::kPresharedKey, peer->preshared_key,
                    &peer_dict);
          SetString(::onc::wireguard::kEndpoint, peer->endpoint, &peer_dict);
          SetString(::onc::wireguard::kAllowedIPs, peer->allowed_ips,
                    &peer_dict);
          if (peer->persistent_keepalive_interval) {
            peer_dict.Set(
                ::onc::wireguard::kPersistentKeepalive,
                base::NumberToString(peer->persistent_keepalive_interval));
          }
          peer_list.Append(std::move(peer_dict));
        }
      }
      wireguard_dict.Set(::onc::wireguard::kPeers, std::move(peer_list));
      wireguard_dict.Set(::onc::vpn::kSaveCredentials, true);
      type_dict.Set(::onc::vpn::kWireGuard, std::move(wireguard_dict));
    }

    if (vpn.type) {
      SetString(::onc::vpn::kType, MojoVpnTypeToOnc(vpn.type->value),
                &type_dict);
    }
  } else if (properties->type_config->is_wifi()) {
    type = mojom::NetworkType::kWiFi;
    const mojom::WiFiConfigProperties& wifi =
        *properties->type_config->get_wifi();
    SetString(::onc::wifi::kPassphrase, wifi.passphrase, &type_dict);
    SetStringIfNotEmpty(::onc::wifi::kSSID, wifi.ssid, &type_dict);
    SetString(::onc::wifi::kPassphrase, wifi.passphrase, &type_dict);

    switch (wifi.hidden_ssid) {
      case mojom::HiddenSsidMode::kDisabled:
        type_dict.Set(::onc::wifi::kHiddenSSID, false);
        break;
      case mojom::HiddenSsidMode::kEnabled:
        type_dict.Set(::onc::wifi::kHiddenSSID, true);
        break;
      case mojom::HiddenSsidMode::kAutomatic:
        // This is expressed to the platform by leaving off kHiddenSSID.
        break;
    }

    SetString(::onc::wifi::kSecurity, MojoSecurityTypeToOnc(wifi.security),
              &type_dict);
    if (wifi.eap) {
      type_dict.Set(::onc::wifi::kEAP, GetEAPProperties(*wifi.eap.get()));
    }
  }

  std::string onc_type = MojoNetworkTypeToOnc(type);
  if (onc_type.empty()) {
    NET_LOG(ERROR) << "Invalid NetworkConfig properties";
    return absl::nullopt;
  }
  SetString(::onc::network_config::kType, onc_type, &onc);

  // Process other |properties| members. Order matches the mojo struct.

  if (properties->ip_address_config_type) {
    onc.Set(::onc::network_config::kIPAddressConfigType,
            *properties->ip_address_config_type);
  }
  if (properties->metered) {
    onc.Set(::onc::network_config::kMetered, properties->metered->value);
  }
  SetString(::onc::network_config::kName, properties->name, &onc);
  SetString(::onc::network_config::kNameServersConfigType,
            properties->name_servers_config_type, &onc);

  if (properties->priority) {
    onc.Set(::onc::network_config::kPriority, properties->priority->value);
  }

  if (properties->proxy_settings) {
    const mojom::ProxySettings& proxy = *properties->proxy_settings;
    base::Value::Dict proxy_dict;
    proxy_dict.Set(::onc::proxy::kType, proxy.type);
    if (proxy.manual) {
      const mojom::ManualProxySettings& manual = *proxy.manual;
      base::Value::Dict manual_dict;
      SetProxyLocation(::onc::proxy::kHttp, manual.http_proxy, &manual_dict);
      SetProxyLocation(::onc::proxy::kHttps, manual.secure_http_proxy,
                       &manual_dict);
      SetProxyLocation(::onc::proxy::kFtp, manual.ftp_proxy, &manual_dict);
      SetProxyLocation(::onc::proxy::kSocks, manual.socks, &manual_dict);
      proxy_dict.Set(::onc::proxy::kManual, std::move(manual_dict));
    }
    SetStringList(::onc::proxy::kExcludeDomains, proxy.exclude_domains,
                  &proxy_dict);
    SetString(::onc::proxy::kPAC, proxy.pac, &proxy_dict);
    onc.Set(::onc::network_config::kProxySettings, std::move(proxy_dict));
  }

  if (properties->static_ip_config) {
    const mojom::IPConfigProperties& ip_config = *properties->static_ip_config;
    base::Value::Dict ip_config_dict;
    SetString(::onc::ipconfig::kGateway, ip_config.gateway, &ip_config_dict);
    SetString(::onc::ipconfig::kIPAddress, ip_config.ip_address,
              &ip_config_dict);
    SetStringList(::onc::ipconfig::kNameServers, ip_config.name_servers,
                  &ip_config_dict);
    ip_config_dict.Set(::onc::ipconfig::kRoutingPrefix,
                       ip_config.routing_prefix);
    ip_config_dict.Set(::onc::ipconfig::kType,
                       MojoIPConfigTypeToOnc(ip_config.type));
    SetString(::onc::ipconfig::kWebProxyAutoDiscoveryUrl,
              ip_config.web_proxy_auto_discovery_url, &ip_config_dict);
    onc.Set(::onc::network_config::kStaticIPConfig, std::move(ip_config_dict));
  }

  if (properties->auto_connect) {
    NetworkTypePattern type_pattern = MojoTypeToPattern(type);
    if (type_pattern.Equals(NetworkTypePattern::Cellular()) ||
        type_pattern.Equals(NetworkTypePattern::VPN()) ||
        type_pattern.Equals(NetworkTypePattern::WiFi())) {
      // Note: All type dicts use the same kAutoConnect key.
      type_dict.Set(::onc::wifi::kAutoConnect, properties->auto_connect->value);
    }
  }

  if (!type_dict.empty()) {
    onc.Set(onc_type, std::move(type_dict));
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
  result->available_for_network_auth = cert.available_for_network_auth;
  result->device_wide = cert.device_wide;
  if (type == mojom::CertificateType::kServerCA)
    result->pem_or_id = cert.pem;
  if (type == mojom::CertificateType::kUserCert)
    result->pem_or_id = cert.pkcs11_id;
  return result;
}

mojom::TrafficCounterSource ConvertToTrafficCounterSourceEnum(
    const std::string& source) {
  if (source == shill::kTrafficCounterSourceUnknown)
    return mojom::TrafficCounterSource::kUnknown;
  if (source == shill::kTrafficCounterSourceChrome)
    return mojom::TrafficCounterSource::kChrome;
  if (source == shill::kTrafficCounterSourceUser)
    return mojom::TrafficCounterSource::kUser;
  if (source == shill::kTrafficCounterSourceArc)
    return mojom::TrafficCounterSource::kArc;
  if (source == shill::kTrafficCounterSourceCrosvm)
    return mojom::TrafficCounterSource::kCrosvm;
  if (source == shill::kTrafficCounterSourcePluginvm)
    return mojom::TrafficCounterSource::kPluginvm;
  if (source == shill::kTrafficCounterSourceUpdateEngine)
    return mojom::TrafficCounterSource::kUpdateEngine;
  if (source == shill::kTrafficCounterSourceVpn)
    return mojom::TrafficCounterSource::kVpn;
  if (source == shill::kTrafficCounterSourceSystem)
    return mojom::TrafficCounterSource::kSystem;
  NOTREACHED() << "Unknown traffic counter source: " << source;
  return mojom::TrafficCounterSource::kUnknown;
}

}  // namespace

CrosNetworkConfig::CrosNetworkConfig()
    : CrosNetworkConfig(
          NetworkHandler::Get()->network_state_handler(),
          NetworkHandler::Get()->network_device_handler(),
          NetworkHandler::Get()->cellular_inhibitor(),
          NetworkHandler::Get()->cellular_esim_profile_handler(),
          NetworkHandler::Get()->managed_network_configuration_handler(),
          NetworkHandler::Get()->network_connection_handler(),
          NetworkHandler::Get()->network_certificate_handler(),
          NetworkHandler::Get()->network_profile_handler()) {}

CrosNetworkConfig::CrosNetworkConfig(
    NetworkStateHandler* network_state_handler,
    NetworkDeviceHandler* network_device_handler,
    CellularInhibitor* cellular_inhibitor,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    ManagedNetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkCertificateHandler* network_certificate_handler,
    NetworkProfileHandler* network_profile_handler)
    : network_state_handler_(network_state_handler),
      network_device_handler_(network_device_handler),
      cellular_inhibitor_(cellular_inhibitor),
      cellular_esim_profile_handler_(cellular_esim_profile_handler),
      network_configuration_handler_(network_configuration_handler),
      network_connection_handler_(network_connection_handler),
      network_certificate_handler_(network_certificate_handler),
      network_profile_handler_(network_profile_handler) {
  CHECK(network_state_handler);
}

CrosNetworkConfig::~CrosNetworkConfig() {
  if (network_certificate_handler_ &&
      network_certificate_handler_->HasObserver(this)) {
    network_certificate_handler_->RemoveObserver(this);
  }
  if (cellular_inhibitor_ && cellular_inhibitor_->HasObserver(this))
    cellular_inhibitor_->RemoveObserver(this);
  if (network_configuration_handler_ &&
      network_configuration_handler_->HasObserver(this)) {
    network_configuration_handler_->RemoveObserver(this);
  }
}

void CrosNetworkConfig::BindReceiver(
    mojo::PendingReceiver<mojom::CrosNetworkConfig> receiver) {
  NET_LOG(EVENT) << "CrosNetworkConfig::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void CrosNetworkConfig::AddObserver(
    mojo::PendingRemote<mojom::CrosNetworkConfigObserver> observer) {
  if (!network_state_handler_observer_.IsObserving()) {
    network_state_handler_observer_.Observe(network_state_handler_);
  }
  if (network_certificate_handler_ &&
      !network_certificate_handler_->HasObserver(this)) {
    network_certificate_handler_->AddObserver(this);
  }
  if (cellular_inhibitor_ && !cellular_inhibitor_->HasObserver(this))
    cellular_inhibitor_->AddObserver(this);
  if (network_configuration_handler_ &&
      !network_configuration_handler_->HasObserver(this)) {
    network_configuration_handler_->AddObserver(this);
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
  std::move(callback).Run(NetworkStateToMojo(network_state_handler_,
                                             cellular_esim_profile_handler_,
                                             vpn_providers_, network));
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
    mojom::NetworkStatePropertiesPtr mojo_network = NetworkStateToMojo(
        network_state_handler_, cellular_esim_profile_handler_, vpn_providers_,
        network);
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
    mojom::DeviceStatePropertiesPtr mojo_device = DeviceStateToMojo(
        device, network_state_handler_, cellular_inhibitor_, technology_state);
    if (mojo_device)
      result.emplace_back(std::move(mojo_device));
  }

  // Handle VPN state separately because VPN is not considered a device by shill
  // and thus will not be included in the |devices| list returned by network
  // state handler. In the UI code, it is treated as a "device" for consistency.
  // In the UI code, knowing whether a device is prohibited or not is done by
  // checking |device_state| field of the DeviceStateProperties of the
  // corresponding device. A VPN device state is returned if built-in VPN
  // services are prohibited by policy even if no VPN services exist in order to
  // indicate that adding a VPN is prohibited in the UI.
  if (network_state_handler_->FirstNetworkByType(NetworkTypePattern::VPN()) ||
      IsVpnProhibited()) {
    result.emplace_back(GetVpnState());
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

  network_configuration_handler_->GetManagedProperties(
      LoginState::Get()->primary_user_hash(), network->path(),
      base::BindOnce(&CrosNetworkConfig::OnGetManagedProperties,
                     weak_factory_.GetWeakPtr(), std::move(callback), guid));
}

void CrosNetworkConfig::OnGetManagedProperties(
    GetManagedPropertiesCallback callback,
    std::string guid,
    const std::string& service_path,
    absl::optional<base::Value> properties,
    absl::optional<std::string> error) {
  if (!properties) {
    NET_LOG(ERROR) << "GetManagedProperties failed for: " << guid
                   << " Error: " << error.value_or("Failed");
    std::move(callback).Run(nullptr);
    return;
  }
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state) {
    NET_LOG(ERROR) << "Network not found: " << service_path;
    std::move(callback).Run(nullptr);
    return;
  }
  mojom::ManagedPropertiesPtr managed_properties = ManagedPropertiesToMojo(
      network_state_handler_, cellular_esim_profile_handler_, network_state,
      vpn_providers_, &properties.value());

  if (managed_properties->type == mojom::NetworkType::kCellular) {
    std::vector<mojom::ApnPropertiesPtr> custom_apn_list =
        GetCustomApnList(guid);
    if (!custom_apn_list.empty()) {
      managed_properties->type_properties->get_cellular()->custom_apn_list =
          std::move(custom_apn_list);
    }
  }

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
    std::move(callback).Run(std::move(managed_properties));
    return;
  }

  // Request the EAP state. On success the EAP state will be applied to
  // |managed_properties| and returned. On failure |managed_properties| will
  // be returned as-is.
  NET_LOG(DEBUG) << "Requesting EAP state for: " + service_path
                 << " from: " << eap_state->path();
  network_configuration_handler_->GetManagedProperties(
      LoginState::Get()->primary_user_hash(), eap_state->path(),
      base::BindOnce(&CrosNetworkConfig::OnGetManagedPropertiesEap,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(managed_properties)));
}

void CrosNetworkConfig::OnGetManagedPropertiesEap(
    GetManagedPropertiesCallback callback,
    mojom::ManagedPropertiesPtr managed_properties,
    const std::string& service_path,
    absl::optional<base::Value> eap_properties,
    absl::optional<std::string> error) {
  if (eap_properties) {
    // Copy the EAP properties to |managed_properties| before sending.
    const base::Value* ethernet_dict =
        eap_properties->FindDictKey(::onc::network_config::kEthernet);
    if (ethernet_dict) {
      auto ethernet = mojom::ManagedEthernetProperties::New();
      ethernet->authentication =
          GetManagedString(ethernet_dict, ::onc::ethernet::kAuthentication);
      ethernet->eap =
          GetManagedEAPProperties(ethernet_dict, ::onc::ethernet::kEAP);
      managed_properties->type_properties =
          mojom::NetworkTypeManagedProperties::NewEthernet(std::move(ethernet));
    }
  }

  std::move(callback).Run(std::move(managed_properties));
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
      return;
    }
    network = eap_state;
  }

  if (!ash::features::IsApnRevampEnabled() &&
      network->type() == shill::kTypeCellular &&
      properties->type_config->is_cellular()) {
    UpdateCustomApnList(network, properties.get());
  }

  absl::optional<base::Value::Dict> onc =
      GetOncFromConfigProperties(properties.get(), guid);
  if (!onc) {
    NET_LOG(ERROR) << "Bad ONC Configuration for " << guid;
    std::move(callback).Run(false, kErrorInvalidONCConfiguration);
    return;
  }

  SetPropertiesInternal(guid, *network, std::move(*onc), std::move(callback));
}

void CrosNetworkConfig::SetPropertiesInternal(const std::string& guid,
                                              const NetworkState& network,
                                              base::Value::Dict onc,
                                              SetPropertiesCallback callback) {
  NET_LOG(DEBUG) << "Configuring properties for " << guid << ": " << onc;

  int callback_id = callback_id_++;
  set_properties_callbacks_[callback_id] = std::move(callback);

  // If the profile path is empty the network is not saved, so we need to call
  // CreateConfiguration(). This can happen for EthernetEAP where a default
  // service is generated by Shill but may not be saved.
  if (network.profile_path().empty()) {
    NET_LOG(USER) << "Configuring properties for " << guid
                  << " (no profile entry set)";
    std::string user_id_hash = LoginState::Get()->primary_user_hash();

    network_configuration_handler_->CreateConfiguration(
        user_id_hash, base::Value(std::move(onc)),
        base::BindOnce(&CrosNetworkConfig::SetPropertiesConfigureSuccess,
                       weak_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&CrosNetworkConfig::SetPropertiesFailure,
                       weak_factory_.GetWeakPtr(), guid, callback_id));
    return;
  }

  network_configuration_handler_->SetProperties(
      network.path(), base::Value(std::move(onc)),
      base::BindOnce(&CrosNetworkConfig::SetPropertiesSuccess,
                     weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&CrosNetworkConfig::SetPropertiesFailure,
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

void CrosNetworkConfig::SetPropertiesFailure(const std::string& guid,
                                             int callback_id,
                                             const std::string& error_name) {
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
    std::move(callback).Run(/*guid=*/absl::nullopt, kErrorNotReady);
    return;
  }

  if (!shared && UserManager::Get()->GetPrimaryUser() !=
                     UserManager::Get()->GetActiveUser()) {
    NET_LOG(ERROR)
        << "Attempt to set unshared configuration from non primary user";
    std::move(callback).Run(/*guid=*/absl::nullopt, kErrorAccessToSharedConfig);
    return;
  }

  absl::optional<base::Value::Dict> onc =
      GetOncFromConfigProperties(properties.get(), /*guid=*/absl::nullopt);
  if (!onc) {
    std::move(callback).Run(/*guid=*/absl::nullopt,
                            kErrorInvalidONCConfiguration);
    return;
  }

  std::string user_id_hash =
      shared ? "" : LoginState::Get()->primary_user_hash();

  int callback_id = callback_id_++;
  configure_network_callbacks_[callback_id] = std::move(callback);

  network_configuration_handler_->CreateConfiguration(
      user_id_hash, base::Value(std::move(*onc)),
      base::BindOnce(&CrosNetworkConfig::ConfigureNetworkSuccess,
                     weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&CrosNetworkConfig::ConfigureNetworkFailure,
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

void CrosNetworkConfig::ConfigureNetworkFailure(int callback_id,
                                                const std::string& error_name) {
  auto iter = configure_network_callbacks_.find(callback_id);
  DCHECK(iter != configure_network_callbacks_.end());
  DCHECK(iter->second);
  NET_LOG(ERROR) << "Failed to configure network. Error: " << error_name;
  std::move(iter->second).Run(/*guid=*/absl::nullopt, error_name);
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
        base::BindOnce(&CrosNetworkConfig::ForgetNetworkSuccess,
                       weak_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&CrosNetworkConfig::ForgetNetworkFailure,
                       weak_factory_.GetWeakPtr(), guid, callback_id));
  } else {
    network_configuration_handler_->RemoveConfigurationFromCurrentProfile(
        network->path(),
        base::BindOnce(&CrosNetworkConfig::ForgetNetworkSuccess,
                       weak_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&CrosNetworkConfig::ForgetNetworkFailure,
                       weak_factory_.GetWeakPtr(), guid, callback_id));
  }
}

void CrosNetworkConfig::ForgetNetworkSuccess(int callback_id) {
  auto iter = forget_network_callbacks_.find(callback_id);
  DCHECK(iter != forget_network_callbacks_.end());
  std::move(iter->second).Run(/*success=*/true);
  forget_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::ForgetNetworkFailure(const std::string& guid,
                                             int callback_id,
                                             const std::string& error_name) {
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

  NET_LOG(USER) << __func__ << " " << type << ":" << enabled;

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
        base::BindOnce(&CrosNetworkConfig::SetCellularSimStateSuccess,
                       weak_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&CrosNetworkConfig::SetCellularSimStateFailure,
                       weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  if (lock_type == shill::kSIMLockPin) {
    // Unlock locked SIM.
    network_device_handler_->EnterPin(
        device_state->path(), sim_state->current_pin_or_puk,
        base::BindOnce(&CrosNetworkConfig::SetCellularSimStateSuccess,
                       weak_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&CrosNetworkConfig::SetCellularSimStateFailure,
                       weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  if (sim_state->new_pin) {
    // Change the SIM PIN.
    network_device_handler_->ChangePin(
        device_state->path(), sim_state->current_pin_or_puk,
        *sim_state->new_pin,
        base::BindOnce(&CrosNetworkConfig::SetCellularSimStateSuccess,
                       weak_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&CrosNetworkConfig::SetCellularSimStateFailure,
                       weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  // Enable or disable SIM locking.
  network_device_handler_->RequirePin(
      device_state->path(), sim_state->require_pin,
      sim_state->current_pin_or_puk,
      base::BindOnce(&CrosNetworkConfig::SetCellularSimStateSuccess,
                     weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&CrosNetworkConfig::SetCellularSimStateFailure,
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
    const std::string& error_name) {
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
      base::BindOnce(&CrosNetworkConfig::SelectCellularMobileNetworkSuccess,
                     weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&CrosNetworkConfig::SelectCellularMobileNetworkFailure,
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
    const std::string& error_name) {
  auto iter = select_cellular_mobile_network_callbacks_.find(callback_id);
  DCHECK(iter != select_cellular_mobile_network_callbacks_.end());
  std::move(iter->second).Run(false);
  select_cellular_mobile_network_callbacks_.erase(iter);
}

void CrosNetworkConfig::UpdateCustomApnList(
    const NetworkState* network,
    const mojom::ConfigProperties* properties) {
  DCHECK(!ash::features::IsApnRevampEnabled());

  const mojom::CellularConfigProperties& cellular_config =
      *properties->type_config->get_cellular();
  if (!cellular_config.apn) {
    return;
  }
  const DeviceState* device =
      network_state_handler_->GetDeviceState(network->device_path());
  if (!device) {
    // Unexpected, but see note in NetworkStateToMojo.
    NET_LOG(DEBUG) << "Cellular device is not available for APN list: "
                   << network->device_path();
    return;
  }
  // Do not update custom APN list if APN is in device APN list.
  if (device->HasAPN(cellular_config.apn->access_point_name)) {
    return;
  }

  // The pre-revamp UI only supports setting a single custom apn.
  base::Value::List custom_apn_list;
  custom_apn_list.Append(MojoApnToOnc(*cellular_config.apn));

  NET_LOG(DEBUG) << "Saving Custom APN entry for " << network->guid();
  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  network_metadata_store->SetCustomApnList(network->guid(),
                                           std::move(custom_apn_list));
}

std::vector<mojom::ApnPropertiesPtr> CrosNetworkConfig::GetCustomApnList(
    const std::string& guid) {
  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  std::vector<mojom::ApnPropertiesPtr> mojo_custom_apns;
  const base::Value::List* custom_apn_list =
      network_metadata_store->GetCustomApnList(guid);
  if (!custom_apn_list) {
    return mojo_custom_apns;
  }
  for (const auto& apn : *custom_apn_list) {
    DCHECK(apn.is_dict());
    mojom::ApnPropertiesPtr mojo_apn = mojom::ApnProperties::New();
    mojo_apn->access_point_name =
        GetRequiredString(&apn, ::onc::cellular_apn::kAccessPointName);
    mojo_apn->name = GetString(&apn, ::onc::cellular_apn::kName);
    mojo_apn->username = GetString(&apn, ::onc::cellular_apn::kUsername);
    mojo_apn->password = GetString(&apn, ::onc::cellular_apn::kPassword);
    mojo_apn->authentication =
        GetString(&apn, ::onc::cellular_apn::kAuthentication);
    mojo_apn->localized_name =
        GetString(&apn, ::onc::cellular_apn::kLocalizedName);
    mojo_apn->language = GetString(&apn, ::onc::cellular_apn::kLanguage);
    mojo_apn->attach = GetString(&apn, ::onc::cellular_apn::kAttach);

    if (ash::features::IsApnRevampEnabled()) {
      mojo_apn->id = GetString(&apn, ::onc::cellular_apn::kId);
      mojo_apn->state = OncApnStateTypeToMojo(
          base::OptionalToPtr(GetString(&apn, ::onc::cellular_apn::kState)));
      // TODO(b/162365553) Remove missing value checking after Shill implements
      // the interface.
      if (!GetString(&apn, ::onc::cellular_apn::kAuthenticationType)) {
        mojo_apn->authentication_type =
            mojom::ApnAuthenticationType::kAutomatic;
      } else {
        mojo_apn->authentication_type = OncApnAuthenticationTypeToMojo(
            GetRequiredString(&apn, ::onc::cellular_apn::kAuthenticationType));
      }
      // TODO(b/162365553) Remove missing value checking after Shill implements
      // the interface.
      if (!GetString(&apn, ::onc::cellular_apn::kIpType)) {
        mojo_apn->ip_type = mojom::ApnIpType::kAutomatic;
      } else {
        mojo_apn->ip_type = OncApnIpTypeToMojo(
            GetRequiredString(&apn, ::onc::cellular_apn::kIpType));
      }
      // TODO(b/162365553) Remove missing value checking after Shill implements
      // the interface.
      if (!GetStringList(&apn, ::onc::cellular_apn::kApnTypes)) {
        mojo_apn->apn_types = {mojom::ApnType::kDefault};
      } else {
        mojo_apn->apn_types = OncApnTypesToMojo(
            GetRequiredStringList(&apn, ::onc::cellular_apn::kApnTypes));
      }
    }

    mojo_custom_apns.push_back(std::move(mojo_apn));
  }
  return mojo_custom_apns;
}

void CrosNetworkConfig::RequestNetworkScan(mojom::NetworkType type) {
  network_state_handler_->RequestScan(MojoTypeToPattern(type));
}

void CrosNetworkConfig::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  auto result = mojom::GlobalPolicy::New();

  // Network configuration handler can be nullptr in tests.
  if (!network_configuration_handler_) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Global network configuration policy values come from the device policy.
  const base::Value* global_policy_dict =
      network_configuration_handler_->GetGlobalConfigFromPolicy(
          /*userhash=*/std::string());
  if (!global_policy_dict) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Sets mojom global policy results directly from the |global_policy_dict|.
  // If there is no key (in the case of non-managed devices), the default
  // mojom::GlobalPolicy() boolean value(s) specified explicitly in
  // cros_network_config.mojom is used instead.
  result->allow_cellular_sim_lock = GetBoolean(
      global_policy_dict, ::onc::global_network_config::kAllowCellularSimLock,
      /*value_if_key_missing_from_dict=*/result->allow_cellular_sim_lock);
  result->allow_only_policy_cellular_networks =
      GetBoolean(global_policy_dict,
                 ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks,
                 /*value_if_key_missing_from_dict=*/
                 result->allow_only_policy_cellular_networks);
  result->allow_only_policy_networks_to_autoconnect = GetBoolean(
      global_policy_dict,
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      /*value_if_key_missing_from_dict=*/
      result->allow_only_policy_networks_to_autoconnect);
  result->allow_only_policy_wifi_networks_to_connect =
      GetBoolean(global_policy_dict,
                 ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnect,
                 /*value_if_key_missing_from_dict=*/
                 result->allow_only_policy_wifi_networks_to_connect);
  result->allow_only_policy_wifi_networks_to_connect_if_available = GetBoolean(
      global_policy_dict,
      ::onc::global_network_config::kAllowOnlyPolicyWiFiToConnectIfAvailable,
      /*value_if_key_missing_from_dict=*/
      result->allow_only_policy_wifi_networks_to_connect_if_available);
  absl::optional<std::vector<std::string>> blocked_hex_ssids = GetStringList(
      global_policy_dict, ::onc::global_network_config::kBlockedHexSSIDs);
  if (blocked_hex_ssids)
    result->blocked_hex_ssids = std::move(*blocked_hex_ssids);

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
      base::BindOnce(&CrosNetworkConfig::StartConnectSuccess,
                     weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&CrosNetworkConfig::StartConnectFailure,
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

void CrosNetworkConfig::StartConnectFailure(int callback_id,
                                            const std::string& error_name) {
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
      base::BindOnce(&CrosNetworkConfig::StartDisconnectSuccess,
                     weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&CrosNetworkConfig::StartDisconnectFailure,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CrosNetworkConfig::StartDisconnectSuccess(int callback_id) {
  auto iter = start_disconnect_callbacks_.find(callback_id);
  DCHECK(iter != start_disconnect_callbacks_.end());
  std::move(iter->second).Run(true);
  start_disconnect_callbacks_.erase(iter);
}

void CrosNetworkConfig::StartDisconnectFailure(int callback_id,
                                               const std::string& error_name) {
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

void CrosNetworkConfig::GetAlwaysOnVpn(GetAlwaysOnVpnCallback callback) {
  const NetworkProfile* profile =
      network_profile_handler_->GetDefaultUserProfile();
  if (!profile) {
    NET_LOG(ERROR) << "GetAlwaysOnVpn: no user profile found";
    // No profile available, ensure the callback gets fired with always-on VPN
    // disabled.
    OnGetAlwaysOnVpn(std::move(callback), shill::kAlwaysOnVpnModeOff,
                     std::string());
    return;
  }

  network_profile_handler_->GetAlwaysOnVpnConfiguration(
      profile->path,
      base::BindOnce(&CrosNetworkConfig::OnGetAlwaysOnVpn,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrosNetworkConfig::OnGetAlwaysOnVpn(GetAlwaysOnVpnCallback callback,
                                         std::string mode,
                                         std::string service_path) {
  mojom::AlwaysOnVpnMode vpn_mode;
  if (mode == shill::kAlwaysOnVpnModeOff) {
    vpn_mode = mojom::AlwaysOnVpnMode::kOff;
  } else if (mode == shill::kAlwaysOnVpnModeBestEffort) {
    vpn_mode = mojom::AlwaysOnVpnMode::kBestEffort;
  } else if (mode == shill::kAlwaysOnVpnModeStrict) {
    vpn_mode = mojom::AlwaysOnVpnMode::kStrict;
  } else {
    NOTREACHED() << "OnGetAlwaysOnVpn: invalid always-on VPN mode: " << mode;
    vpn_mode = mojom::AlwaysOnVpnMode::kOff;
  }

  std::string guid;
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  // |network| is expected to be null when the service has not been set (yet)
  // or has been removed.
  if (network) {
    guid = network->guid();
  }

  mojom::AlwaysOnVpnPropertiesPtr properties =
      mojom::AlwaysOnVpnProperties::New(vpn_mode, guid);
  std::move(callback).Run(std::move(properties));
}

void CrosNetworkConfig::SetAlwaysOnVpn(
    mojom::AlwaysOnVpnPropertiesPtr properties) {
  const NetworkProfile* profile =
      network_profile_handler_->GetDefaultUserProfile();
  if (!profile) {
    NET_LOG(ERROR) << "SetAlwaysOnVpn: no user profile found";
    return;
  }

  std::string mode;
  switch (properties->mode) {
    case mojom::AlwaysOnVpnMode::kBestEffort:
      mode = shill::kAlwaysOnVpnModeBestEffort;
      break;
    case mojom::AlwaysOnVpnMode::kStrict:
      mode = shill::kAlwaysOnVpnModeStrict;
      break;
    case mojom::AlwaysOnVpnMode::kOff:
      mode = shill::kAlwaysOnVpnModeOff;
      break;
    default:
      NOTREACHED() << "SetAlwaysOnVpn: invalid mode: " << properties->mode;
      return;
  }
  network_profile_handler_->SetAlwaysOnVpnMode(profile->path, mode);

  if (properties->service_guid.empty()) {
    return;
  }
  std::string service_path = GetServicePathFromGuid(properties->service_guid);
  if (service_path.empty()) {
    return;
  }
  network_profile_handler_->SetAlwaysOnVpnService(profile->path, service_path);
}

void CrosNetworkConfig::GetSupportedVpnTypes(
    GetSupportedVpnTypesCallback callback) {
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&CrosNetworkConfig::OnGetSupportedVpnTypes,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrosNetworkConfig::OnGetSupportedVpnTypes(
    GetSupportedVpnTypesCallback callback,
    absl::optional<base::Value> properties) {
  std::vector<std::string> result;
  if (!properties) {
    NET_LOG(ERROR) << "GetSupportedVpnTypes: GetProperties failed.";
    std::move(callback).Run(result);
    return;
  }
  const base::Value* value =
      properties->FindKey(shill::kSupportedVPNTypesProperty);
  if (value) {
    result =
        base::SplitString(*value->GetIfString(), ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
  }
  std::move(callback).Run(result);
}

void CrosNetworkConfig::RequestTrafficCounters(
    const std::string& guid,
    RequestTrafficCountersCallback callback) {
  std::string service_path = GetServicePathFromGuid(guid);
  if (service_path.empty()) {
    NET_LOG(ERROR) << "RequestTrafficCounters: service path for guid " << guid
                   << " not found";
    std::move(callback).Run({});
    return;
  }
  network_state_handler_->RequestTrafficCounters(
      service_path,
      base::BindOnce(&CrosNetworkConfig::PopulateTrafficCounters,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrosNetworkConfig::PopulateTrafficCounters(
    RequestTrafficCountersCallback callback,
    absl::optional<base::Value> traffic_counters) {
  if (!traffic_counters || !traffic_counters->is_list() ||
      !traffic_counters->GetList().size()) {
    std::move(callback).Run({});
    return;
  }
  std::vector<mojom::TrafficCounterPtr> counters;
  for (const base::Value& tc : traffic_counters->GetList()) {
    DCHECK(tc.is_dict());
    const base::Value* source =
        tc.FindKeyOfType("source", base::Value::Type::STRING);
    DCHECK(source);

    // Since rx_bytes may be larger than the maximum value representable by
    // uint32_t, we must check whether it was implicitly converted to a double
    // during D-Bus deserialization.
    uint64_t rx_bytes;
    const base::Value* rb = tc.FindKey("rx_bytes");
    DCHECK(rb);
    if (rb->type() == base::Value::Type::INTEGER) {
      rx_bytes = rb->GetInt();
    } else if (rb->type() == base::Value::Type::DOUBLE) {
      rx_bytes = std::floor(rb->GetDouble());
    } else {
      NOTREACHED();
    }

    // Since tx_bytes may be larger than the maximum value representable by
    // uint32_t, we must check whether it was implicitly converted to a double
    // during D-Bus deserialization.
    uint64_t tx_bytes;
    const base::Value* tb = tc.FindKey("tx_bytes");
    DCHECK(tb);
    if (tb->type() == base::Value::Type::INTEGER) {
      tx_bytes = tb->GetInt();
    } else if (tb->type() == base::Value::Type::DOUBLE) {
      tx_bytes = std::floor(tb->GetDouble());
    } else {
      NOTREACHED();
    }

    counters.push_back(
        mojom::TrafficCounter::New(ConvertToTrafficCounterSourceEnum(
                                       base::ToLowerASCII(source->GetString())),
                                   rx_bytes, tx_bytes));
  }
  std::move(callback).Run(std::move(counters));
}

void CrosNetworkConfig::ResetTrafficCounters(const std::string& guid) {
  std::string service_path = GetServicePathFromGuid(guid);
  if (service_path.empty()) {
    NET_LOG(ERROR) << "ResetTrafficCounters: service path for guid " << guid
                   << " not found";
    return;
  }
  network_state_handler_->ResetTrafficCounters(service_path);
}

void CrosNetworkConfig::SetTrafficCountersAutoReset(
    const std::string& guid,
    bool auto_reset,
    mojom::UInt32ValuePtr day,
    SetTrafficCountersAutoResetCallback callback) {
  if (day && !auto_reset) {
    NET_LOG(ERROR) << "Failed to set auto reset day for " << guid
                   << ": auto reset must be enabled.";
    std::move(callback).Run(false);
    return;
  }
  if (!day && auto_reset) {
    NET_LOG(ERROR) << "Failed to enable auto reset for " << guid << ": a valid "
                   << "day between 1 and 31 (inclusive) must be provided.";
    std::move(callback).Run(false);
    return;
  }
  if (day && (day->value < 1 || day->value > 31)) {
    NET_LOG(ERROR) << "Failed to set auto reset day " << day->value << " for "
                   << guid << ": day must be between 1 and 31 (inclusive)";
    std::move(callback).Run(false);
    return;
  }
  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetEnableTrafficCountersAutoReset(guid, auto_reset);
  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetDayOfTrafficCountersAutoReset(
          guid, day ? absl::optional<int>(day->value) : absl::nullopt);
  std::move(callback).Run(true);
}

void CrosNetworkConfig::CreateCustomApn(const std::string& network_guid,
                                        mojom::ApnPropertiesPtr apn) {
  if (!ash::features::IsApnRevampEnabled()) {
    receivers_.ReportBadMessage(
        "CreateCustomApn cannot be called if the APN Revamp feature flag is "
        "disabled.");
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);
  if (!network || network->profile_path().empty()) {
    NET_LOG(ERROR) << "CreateCustomApn: Called with unconfigured network: "
                   << network_guid << ".";
    ash::CellularNetworkMetricsLogger::LogCreateCustomApnResult(
        /*success=*/false, std::move(apn));
    return;
  }

  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  DCHECK(network_metadata_store);

  base::Value::List new_apns;
  if (const base::Value::List* old_custom_apns =
          network_metadata_store->GetCustomApnList(network_guid)) {
    if (old_custom_apns->size() >= mojom::kMaxNumCustomApns) {
      NET_LOG(ERROR)
          << "CreateCustomApn: Cannot create new custom APN for network: "
          << network_guid << ". Network already has the max amount allowed: "
          << mojom::kMaxNumCustomApns;
      return;
    }

    new_apns = old_custom_apns->Clone();
  }

  // Set unique Id for custom APNs
  apn->id = base::Token::CreateRandom().ToString();
  // Insert the new custom APN at the beginning of the list to store them by
  // insertion order
  new_apns.Insert(new_apns.begin(), base::Value(MojoApnToOnc(*apn)));

  NET_LOG(USER) << "CreateCustomApn: Setting user APNs for: " << network_guid
                << ": " << new_apns.size();

  network_metadata_store->SetCustomApnList(network_guid, new_apns.Clone());

  SetPropertiesInternal(
      network_guid, *network, UserApnListToOnc(network_guid, &new_apns),
      base::BindOnce(
          [](const std::string& guid, mojom::ApnPropertiesPtr apn, bool success,
             const std::string& message) {
            if (!success) {
              NET_LOG(ERROR)
                  << "CreateCustomApn: Failed to update the user APN "
                     "list in Shill for network: "
                  << guid << ": [" << message << ']';
            }
            ash::CellularNetworkMetricsLogger::LogCreateCustomApnResult(
                success, std::move(apn));
          },
          network_guid, std::move(apn)));
}

void CrosNetworkConfig::RemoveCustomApn(const std::string& network_guid,
                                        const std::string& apn_id) {
  if (!ash::features::IsApnRevampEnabled()) {
    receivers_.ReportBadMessage(
        "RemoveCustomApn: Cannot be called if the APN Revamp feature flag is "
        "disabled.");
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);
  if (!network || network->profile_path().empty()) {
    NET_LOG(ERROR) << "RemoveCustomApn: Called with unconfigured network: "
                   << network_guid << ".";
    ash::CellularNetworkMetricsLogger::LogRemoveCustomApnResult(
        /*success=*/false, /*apn_types=*/{});
    return;
  }

  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  DCHECK(network_metadata_store);

  const base::Value::List* current_apns =
      network_metadata_store->GetCustomApnList(network_guid);
  if (!current_apns || current_apns->empty()) {
    NET_LOG(ERROR) << "RemoveCustomApn: Called for network: " << network_guid
                   << " that does not have any user APNs.";
    ash::CellularNetworkMetricsLogger::LogRemoveCustomApnResult(
        /*success=*/false, /*apn_types=*/{});
    return;
  }

  base::Value::List new_apns = current_apns->Clone();
  std::vector<mojom::ApnType> removed_apn_apn_types;
  if (!new_apns.EraseIf([&apn_id,
                         &removed_apn_apn_types](const base::Value& item) {
        const std::string* item_id =
            item.GetDict().FindString(::onc::cellular_apn::kId);
        if (item_id && apn_id == *item_id) {
          removed_apn_apn_types = OncApnTypesToMojo(
              GetRequiredStringList(&item, ::onc::cellular_apn::kApnTypes));
          return true;
        }
        return false;
      })) {
    NET_LOG(ERROR) << "RemoveCustomApn: Called for network: " << network_guid
                   << " that does have an user APNs with id: " << apn_id << '.';
    ash::CellularNetworkMetricsLogger::LogRemoveCustomApnResult(
        /*success=*/false, std::move(removed_apn_apn_types));
    return;
  }
  NET_LOG(USER) << "RemoveCustomApn: Setting user APNs for: " << network_guid
                << ": " << new_apns.size();

  network_metadata_store->SetCustomApnList(network_guid, new_apns.Clone());
  SetPropertiesInternal(
      network_guid, *network, UserApnListToOnc(network_guid, &new_apns),
      base::BindOnce(
          [](const std::string& guid, std::vector<mojom::ApnType> apn_types,
             bool success, const std::string& message) {
            if (!success) {
              NET_LOG(ERROR)
                  << "RemoveCustomApn: Failed to update the user APN "
                     "list in Shill for network: "
                  << guid << ": [" << message << ']';
            }
            ash::CellularNetworkMetricsLogger::LogRemoveCustomApnResult(
                success, std::move(apn_types));
          },
          network_guid, std::move(removed_apn_apn_types)));
}

void CrosNetworkConfig::ModifyCustomApn(const std::string& network_guid,
                                        mojom::ApnPropertiesPtr apn) {
  if (!ash::features::IsApnRevampEnabled()) {
    receivers_.ReportBadMessage(
        "ModifyCustomApn: Cannot be called if the APN Revamp feature flag is "
        "disabled.");
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);
  if (!network || network->profile_path().empty()) {
    NET_LOG(ERROR) << "ModifyCustomApn: Called with unconfigured network: "
                   << network_guid << ".";
    return;
  }

  if (!apn->id.has_value()) {
    NET_LOG(ERROR)
        << "ModifyCustomApn: Called with an APN without ID for network: "
        << network_guid << '.';
    return;
  }

  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  DCHECK(network_metadata_store);

  const base::Value::List* old_custom_apns =
      network_metadata_store->GetCustomApnList(network_guid);
  if (!old_custom_apns || old_custom_apns->empty()) {
    NET_LOG(ERROR) << "ModifyCustomApn: Called for network: " << network_guid
                   << " that does not have any user APNs.";
    return;
  }

  base::Value::List new_custom_apns;
  bool was_value_replaced = false;
  for (const base::Value& old_apn : *old_custom_apns) {
    const std::string* old_apn_id =
        old_apn.GetDict().FindString(::onc::cellular_apn::kId);
    DCHECK(old_apn_id);
    if (*apn->id == *old_apn_id) {
      new_custom_apns.Append(MojoApnToOnc(*apn));
      was_value_replaced = true;
    } else {
      new_custom_apns.Append(old_apn.Clone());
    }
  }

  if (!was_value_replaced) {
    NET_LOG(ERROR) << "ModifyCustomApn: Called for network: " << network_guid
                   << " that does have an user APNs with id: " << *apn->id
                   << '.';
    return;
  }
  NET_LOG(USER) << "ModifyCustomApn: Setting user APNs for: " << network_guid
                << ": " << new_custom_apns.size();

  network_metadata_store->SetCustomApnList(network_guid,
                                           new_custom_apns.Clone());
  SetPropertiesInternal(
      network_guid, *network, UserApnListToOnc(network_guid, &new_custom_apns),
      base::BindOnce(
          [](const std::string& guid, bool success,
             const std::string& message) {
            if (!success) {
              NET_LOG(ERROR)
                  << "ModifyCustomApn: Failed to update the user APN "
                     "list in Shill for network: "
                  << guid << ": [" << message << ']';
            }
          },
          network_guid));
}

// static
mojom::TrafficCounterSource CrosNetworkConfig::GetTrafficCounterEnumForTesting(
    const std::string& source) {
  return ConvertToTrafficCounterSourceEnum(source);
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
    mojom::NetworkStatePropertiesPtr mojo_network = NetworkStateToMojo(
        network_state_handler_, cellular_esim_profile_handler_, vpn_providers_,
        network);
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
      NetworkStateToMojo(network_state_handler_, cellular_esim_profile_handler_,
                         vpn_providers_, network);
  if (!mojo_network)
    return;
  for (auto& observer : observers_)
    observer->OnNetworkStateChanged(mojo_network.Clone());
}

void CrosNetworkConfig::DevicePropertiesUpdated(const DeviceState* device) {
  DeviceListChanged();
}

void CrosNetworkConfig::ScanCompleted(const DeviceState* device) {
  DeviceListChanged();
}

void CrosNetworkConfig::ScanStarted(const DeviceState* device) {
  DeviceListChanged();
}

void CrosNetworkConfig::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!network->Matches(NetworkTypePattern::Cellular())) {
    return;
  }
  // inhibit_reason device property is dependent on network connection state of
  // cellular networks. Notify device list change so that clients will update
  // with new inhibit reason.
  DeviceListChanged();
}

void CrosNetworkConfig::OnShuttingDown() {
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
}

void CrosNetworkConfig::OnCertificatesChanged() {
  for (auto& observer : observers_)
    observer->OnNetworkCertificatesChanged();
}

void CrosNetworkConfig::OnInhibitStateChanged() {
  DeviceListChanged();
}

void CrosNetworkConfig::PoliciesApplied(const std::string& userhash) {
  for (auto& observer : observers_)
    observer->OnPoliciesApplied(userhash);
}

void CrosNetworkConfig::OnManagedNetworkConfigurationHandlerShuttingDown() {
  if (network_configuration_handler_ &&
      network_configuration_handler_->HasObserver(this)) {
    network_configuration_handler_->RemoveObserver(this);
  }
  network_configuration_handler_ = nullptr;
}

const std::string& CrosNetworkConfig::GetServicePathFromGuid(
    const std::string& guid) {
  const chromeos::NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  return network ? network->path() : base::EmptyString();
}

}  // namespace network_config
}  // namespace chromeos
