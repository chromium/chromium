// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/arc_net_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/cxx20_erase.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

constexpr int kGetNetworksListLimit = 100;

chromeos::NetworkStateHandler* GetStateHandler() {
  return chromeos::NetworkHandler::Get()->network_state_handler();
}

chromeos::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return chromeos::NetworkHandler::Get()
      ->managed_network_configuration_handler();
}

chromeos::NetworkConnectionHandler* GetNetworkConnectionHandler() {
  return chromeos::NetworkHandler::Get()->network_connection_handler();
}

std::vector<const chromeos::NetworkState*> GetActiveNetworks() {
  std::vector<const chromeos::NetworkState*> active_networks;
  GetStateHandler()->GetActiveNetworkListByType(
      chromeos::NetworkTypePattern::Default(), &active_networks);
  return active_networks;
}

bool IsDeviceOwner() {
  // Check whether the logged-in Chrome OS user is allowed to add or remove WiFi
  // networks. The user account state changes immediately after boot. There is a
  // small window when this may return an incorrect state. However, after things
  // settle down this is guranteed to reflect the correct user account state.
  return user_manager::UserManager::Get()->GetActiveUser()->GetAccountId() ==
         user_manager::UserManager::Get()->GetOwnerAccountId();
}

arc::mojom::SecurityType TranslateWiFiSecurity(const std::string& type) {
  if (type == shill::kSecurityNone)
    return arc::mojom::SecurityType::NONE;
  if (type == shill::kSecurityWep)
    return arc::mojom::SecurityType::WEP_PSK;
  if (type == shill::kSecurityPsk)
    return arc::mojom::SecurityType::WPA_PSK;
  if (type == shill::kSecurityWpa)
    return arc::mojom::SecurityType::WPA_PSK;
  if (type == shill::kSecurity8021x)
    return arc::mojom::SecurityType::WPA_EAP;
  // Robust Security Network does not appear to be defined in Android.
  // Approximate it with WPA_EAP
  if (type == shill::kSecurityRsn)
    return arc::mojom::SecurityType::WPA_EAP;
  LOG(WARNING) << "Unknown WiFi security type " << type;
  return arc::mojom::SecurityType::NONE;
}

// Translates a shill connection state into a mojo ConnectionStateType.
// This is effectively the inverse function of shill.Service::GetStateString
// defined in platform2/shill/service.cc, with in addition some of shill's
// connection states translated to the same ConnectionStateType value.
arc::mojom::ConnectionStateType TranslateConnectionState(
    const std::string& state) {
  if (state == shill::kStateReady)
    return arc::mojom::ConnectionStateType::CONNECTED;
  if (state == shill::kStateAssociation || state == shill::kStateConfiguration)
    return arc::mojom::ConnectionStateType::CONNECTING;
  if ((state == shill::kStateIdle) || (state == shill::kStateFailure) ||
      (state == shill::kStateDisconnect) || (state == ""))
    return arc::mojom::ConnectionStateType::NOT_CONNECTED;
  if (chromeos::NetworkState::StateIsPortalled(state))
    return arc::mojom::ConnectionStateType::PORTAL;
  if (state == shill::kStateOnline)
    return arc::mojom::ConnectionStateType::ONLINE;

  // The remaining cases defined in shill dbus-constants are legacy values from
  // Flimflam and are not expected to be encountered. These are: kStateCarrier,
  // kStateActivationFailure, and kStateOffline.
  NOTREACHED() << "Unknown connection state: " << state;
  return arc::mojom::ConnectionStateType::NOT_CONNECTED;
}

bool IsActiveNetworkState(const chromeos::NetworkState* network) {
  if (!network)
    return false;

  const std::string& state = network->connection_state();
  return state == shill::kStateReady || state == shill::kStateOnline ||
         state == shill::kStateAssociation ||
         state == shill::kStateConfiguration || state == shill::kStatePortal ||
         state == shill::kStateNoConnectivity ||
         state == shill::kStateRedirectFound ||
         state == shill::kStatePortalSuspected;
}

arc::mojom::NetworkType TranslateNetworkType(const std::string& type) {
  if (type == shill::kTypeWifi)
    return arc::mojom::NetworkType::WIFI;
  if (type == shill::kTypeVPN)
    return arc::mojom::NetworkType::VPN;
  if (type == shill::kTypeEthernet)
    return arc::mojom::NetworkType::ETHERNET;
  if (type == shill::kTypeEthernetEap)
    return arc::mojom::NetworkType::ETHERNET;
  if (type == shill::kTypeCellular)
    return arc::mojom::NetworkType::CELLULAR;
  NOTREACHED() << "Unknown network type: " << type;
  return arc::mojom::NetworkType::ETHERNET;
}

// Parses a shill IPConfig dictionary and adds the relevant fields to
// the given |network| NetworkConfiguration object.
void AddIpConfiguration(arc::mojom::NetworkConfiguration* network,
                        const base::Value* shill_ipconfig) {
  if (!shill_ipconfig || !shill_ipconfig->is_dict())
    return;

  // Only set the IP address and gateway if both are defined and non empty.
  const auto* address = shill_ipconfig->FindStringPath(shill::kAddressProperty);
  const auto* gateway = shill_ipconfig->FindStringPath(shill::kGatewayProperty);
  const int prefixlen =
      shill_ipconfig->FindIntPath(shill::kPrefixlenProperty).value_or(0);
  if (address && !address->empty() && gateway && !gateway->empty()) {
    if (prefixlen < 64) {
      network->host_ipv4_prefix_length = prefixlen;
      network->host_ipv4_address = *address;
      network->host_ipv4_gateway = *gateway;
    } else {
      network->host_ipv6_prefix_length = prefixlen;
      network->host_ipv6_global_addresses->push_back(*address);
      network->host_ipv6_gateway = *gateway;
    }
  }

  // If the user has overridden DNS with the "Google nameservers" UI options,
  // the kStaticIPConfigProperty object will be empty except for DNS addresses.
  if (const auto* dns_list =
          shill_ipconfig->FindListKey(shill::kNameServersProperty)) {
    for (const auto& dns_value : dns_list->GetList()) {
      const std::string& dns = dns_value.GetString();
      if (dns.empty())
        continue;

      // When manually setting DNS, up to 4 addresses can be specified in the
      // UI. Unspecified entries can show up as 0.0.0.0 and should be removed.
      if (dns == "0.0.0.0")
        continue;

      network->host_dns_addresses->push_back(dns);
    }
  }

  if (const auto* domains =
          shill_ipconfig->FindKey(shill::kSearchDomainsProperty)) {
    if (domains->is_list()) {
      for (const auto& domain : domains->GetList())
        network->host_search_domains->push_back(domain.GetString());
    }
  }

  const int mtu = shill_ipconfig->FindIntPath(shill::kMtuProperty).value_or(0);
  if (mtu > 0)
    network->host_mtu = mtu;
}

arc::mojom::NetworkConfigurationPtr TranslateNetworkProperties(
    const chromeos::NetworkState* network_state,
    const base::Value* shill_dict) {
  auto mojo = arc::mojom::NetworkConfiguration::New();
  // Initialize optional array fields to avoid null guards both here and in ARC.
  mojo->host_ipv6_global_addresses = std::vector<std::string>();
  mojo->host_search_domains = std::vector<std::string>();
  mojo->host_dns_addresses = std::vector<std::string>();
  mojo->connection_state =
      TranslateConnectionState(network_state->connection_state());
  mojo->guid = network_state->guid();
  if (mojo->guid.empty())
    LOG(ERROR) << "Missing GUID property for network " << network_state->path();
  mojo->type = TranslateNetworkType(network_state->type());
  mojo->is_metered =
      shill_dict &&
      shill_dict->FindBoolPath(shill::kMeteredProperty).value_or(false);

  // IP configuration data is added from the properties of the underlying shill
  // Device and shill Service attached to the Device. Device properties are
  // preferred because Service properties cannot have both IPv4 and IPv6
  // configurations at the same time for dual stack networks. It is necessary to
  // fallback on Service properties for networks without a shill Device exposed
  // over DBus (builtin OpenVPN, builtin L2TP client, Chrome extension VPNs),
  // particularly to obtain the DNS server list (b/155129178).
  // A connecting or newly connected network may not immediately have any
  // usable IP config object if IPv4 dhcp or IPv6 autoconf have not completed
  // yet. This case is covered by requesting shill properties asynchronously
  // when chromeos::NetworkStateHandlerObserver::NetworkPropertiesUpdated is
  // called.

  // Add shill's Device properties to the given mojo NetworkConfiguration
  // objects. This adds the network interface and current IP configurations.
  if (const auto* device =
          GetStateHandler()->GetDeviceState(network_state->device_path())) {
    mojo->network_interface = device->interface();
    for (const auto kv : device->ip_configs().DictItems())
      AddIpConfiguration(mojo.get(), &kv.second);
  }

  if (shill_dict) {
    for (const auto* property :
         {shill::kStaticIPConfigProperty, shill::kSavedIPConfigProperty}) {
      AddIpConfiguration(mojo.get(), shill_dict->FindKey(property));
    }
  }

  if (mojo->type == arc::mojom::NetworkType::WIFI) {
    mojo->wifi = arc::mojom::WiFi::New();
    mojo->wifi->bssid = network_state->bssid();
    mojo->wifi->hex_ssid = network_state->GetHexSsid();
    mojo->wifi->security =
        TranslateWiFiSecurity(network_state->security_class());
    mojo->wifi->frequency = network_state->frequency();
    mojo->wifi->hidden_ssid = shill_dict &&
        shill_dict->FindBoolPath(shill::kWifiHiddenSsid).value_or(false);
    mojo->wifi->signal_strength = network_state->signal_strength();
  }

  return mojo;
}

const chromeos::NetworkState* GetShillBackedNetwork(
    const chromeos::NetworkState* network) {
  if (!network)
    return nullptr;

  // Non-Tether networks are already backed by Shill.
  const std::string type = network->type();
  if (type.empty() || !chromeos::NetworkTypePattern::Tether().MatchesType(type))
    return network;

  // Tether networks which are not connected are also not backed by Shill.
  if (!network->IsConnectedState())
    return nullptr;

  // Connected Tether networks delegate to an underlying Wi-Fi network.
  DCHECK(!network->tether_guid().empty());
  return GetStateHandler()->GetNetworkStateFromGuid(network->tether_guid());
}

// Convenience helper for translating a vector of NetworkState objects to a
// vector of mojo NetworkConfiguration objects.
std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkStates(
    const std::string& arc_vpn_path,
    const chromeos::NetworkStateHandler::NetworkStateList& network_states,
    const std::map<std::string, base::Value>& shill_network_properties) {
  std::vector<arc::mojom::NetworkConfigurationPtr> networks;
  for (const chromeos::NetworkState* state : network_states) {
    const std::string& network_path = state->path();
    // Never tell Android about its own VPN.
    if (network_path == arc_vpn_path)
      continue;

    // For tethered networks, the underlying WiFi networks are not part of
    // active networks. Replace any such tethered network with its underlying
    // backing network, because ARC cannot match its datapath with the tethered
    // network configuration.
    state = GetShillBackedNetwork(state);
    if (!state)
      continue;

    const auto it = shill_network_properties.find(network_path);
    const auto* shill_dict =
        (it != shill_network_properties.end()) ? &it->second : nullptr;
    auto network = TranslateNetworkProperties(state, shill_dict);
    network->is_default_network = state == GetStateHandler()->DefaultNetwork();
    network->service_name = network_path;
    networks.push_back(std::move(network));
  }
  return networks;
}

void ForgetNetworkSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void ForgetNetworkFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void StartConnectSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void StartConnectFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void StartDisconnectSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void StartDisconnectFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void ArcVpnSuccessCallback() {}

void ArcVpnErrorCallback(const std::string& operation,
                         const std::string& error_name,
                         std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "ArcVpnErrorCallback: " << operation << ": " << error_name;
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcNetHostImpl.
class ArcNetHostImplFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcNetHostImpl,
          ArcNetHostImplFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcNetHostImplFactory";

  static ArcNetHostImplFactory* GetInstance() {
    return base::Singleton<ArcNetHostImplFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcNetHostImplFactory>;
  ArcNetHostImplFactory() = default;
  ~ArcNetHostImplFactory() override = default;
};

}  // namespace

// static
ArcNetHostImpl* ArcNetHostImpl::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcNetHostImplFactory::GetForBrowserContext(context);
}

// static
ArcNetHostImpl* ArcNetHostImpl::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcNetHostImplFactory::GetForBrowserContextForTesting(context);
}

ArcNetHostImpl::ArcNetHostImpl(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->net()->SetHost(this);
  arc_bridge_service_->net()->AddObserver(this);
}

ArcNetHostImpl::~ArcNetHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (observing_network_state_) {
    GetStateHandler()->RemoveObserver(this, FROM_HERE);
    GetNetworkConnectionHandler()->RemoveObserver(this);
  }
  arc_bridge_service_->net()->RemoveObserver(this);
  arc_bridge_service_->net()->SetHost(nullptr);
}

void ArcNetHostImpl::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
}

void ArcNetHostImpl::OnConnectionReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (chromeos::NetworkHandler::IsInitialized()) {
    GetStateHandler()->AddObserver(this, FROM_HERE);
    GetNetworkConnectionHandler()->AddObserver(this);
    observing_network_state_ = true;
  }

  // If the default network is an ARC VPN, that means Chrome is restarting
  // after a crash but shill still thinks a VPN is connected. Nuke it.
  const chromeos::NetworkState* default_network =
      GetShillBackedNetwork(GetStateHandler()->DefaultNetwork());
  if (default_network && default_network->type() == shill::kTypeVPN &&
      default_network->GetVpnProviderType() == shill::kProviderArcVpn) {
    GetNetworkConnectionHandler()->DisconnectNetwork(
        default_network->path(), base::BindOnce(&ArcVpnSuccessCallback),
        base::BindOnce(&ArcVpnErrorCallback, "disconnecting stale ARC VPN"));
  }
}

void ArcNetHostImpl::OnConnectionClosed() {
  // Make sure shill doesn't leave an ARC VPN connected after Android
  // goes down.
  AndroidVpnStateChanged(arc::mojom::ConnectionStateType::NOT_CONNECTED);

  if (!observing_network_state_)
    return;

  GetStateHandler()->RemoveObserver(this, FROM_HERE);
  GetNetworkConnectionHandler()->RemoveObserver(this);
  observing_network_state_ = false;
}

void ArcNetHostImpl::GetNetworks(mojom::GetNetworksRequestType type,
                                 GetNetworksCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  chromeos::NetworkStateHandler::NetworkStateList network_states;
  if (type == mojom::GetNetworksRequestType::ACTIVE_ONLY) {
    // Retrieve list of currently active networks.
    GetStateHandler()->GetActiveNetworkListByType(
        chromeos::NetworkTypePattern::Default(), &network_states);
  } else {
    // Otherwise retrieve list of configured or visible WiFi networks.
    bool configured_only =
        type == mojom::GetNetworksRequestType::CONFIGURED_ONLY;
    chromeos::NetworkTypePattern network_pattern =
        chromeos::onc::NetworkTypePatternFromOncType(onc::network_type::kWiFi);
    GetStateHandler()->GetNetworkListByType(
        network_pattern, configured_only, !configured_only /* visible_only */,
        kGetNetworksListLimit, &network_states);
  }

  std::vector<mojom::NetworkConfigurationPtr> networks = TranslateNetworkStates(
      arc_vpn_service_path_, network_states, shill_network_properties_);
  std::move(callback).Run(mojom::GetNetworksResponseType::New(
      arc::mojom::NetworkResult::SUCCESS, std::move(networks)));
}

void ArcNetHostImpl::CreateNetworkSuccessCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& service_path,
    const std::string& guid) {
  cached_guid_ = guid;
  cached_service_path_ = service_path;

  std::move(callback).Run(guid);
}

void ArcNetHostImpl::CreateNetworkFailureCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "CreateNetworkFailureCallback: " << error_name;
  std::move(callback).Run(std::string());
}

void ArcNetHostImpl::CreateNetwork(mojom::WifiConfigurationPtr cfg,
                                   CreateNetworkCallback callback) {
  if (!IsDeviceOwner()) {
    LOG(ERROR) << "Only device owner can create WiFi networks";
    std::move(callback).Run(std::string());
    return;
  }

  std::unique_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> wifi_dict(new base::DictionaryValue);

  if (!cfg->hexssid.has_value() || !cfg->details) {
    LOG(ERROR)
        << "Cannot create WiFi network without hex ssid or WiFi properties";
    std::move(callback).Run(std::string());
    return;
  }

  mojom::ConfiguredNetworkDetailsPtr details =
      std::move(cfg->details->get_configured());
  if (!details) {
    LOG(ERROR) << "Cannot create WiFi network without WiFi properties";
    std::move(callback).Run(std::string());
    return;
  }

  properties->SetKey(onc::network_config::kType,
                     base::Value(onc::network_config::kWiFi));
  wifi_dict->SetKey(onc::wifi::kHexSSID, base::Value(cfg->hexssid.value()));
  wifi_dict->SetKey(onc::wifi::kAutoConnect, base::Value(details->autoconnect));
  if (cfg->security.empty()) {
    wifi_dict->SetKey(onc::wifi::kSecurity,
                      base::Value(onc::wifi::kSecurityNone));
  } else {
    wifi_dict->SetKey(onc::wifi::kSecurity, base::Value(cfg->security));
    if (details->passphrase.has_value()) {
      wifi_dict->SetKey(onc::wifi::kPassphrase,
                        base::Value(details->passphrase.value()));
    }
  }
  properties->SetKey(onc::network_config::kWiFi,
                     base::Value::FromUniquePtrValue(std::move(wifi_dict)));

  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::BindOnce(&ArcNetHostImpl::CreateNetworkSuccessCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&ArcNetHostImpl::CreateNetworkFailureCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

bool ArcNetHostImpl::GetNetworkPathFromGuid(const std::string& guid,
                                            std::string* path) {
  const auto* network =
      GetShillBackedNetwork(GetStateHandler()->GetNetworkStateFromGuid(guid));
  if (network) {
    *path = network->path();
    return true;
  }

  if (cached_guid_ == guid) {
    *path = cached_service_path_;
    return true;
  }

  return false;
}

void ArcNetHostImpl::ForgetNetwork(const std::string& guid,
                                   ForgetNetworkCallback callback) {
  if (!IsDeviceOwner()) {
    LOG(ERROR) << "Only device owner can remove WiFi networks";
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  cached_guid_.clear();
  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->RemoveConfiguration(
      path,
      base::BindOnce(&ForgetNetworkSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&ForgetNetworkFailureCallback,
                     std::move(split_callback.second)));
}

void ArcNetHostImpl::StartConnect(const std::string& guid,
                                  StartConnectCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetNetworkConnectionHandler()->ConnectToNetwork(
      path,
      base::BindOnce(&StartConnectSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&StartConnectFailureCallback,
                     std::move(split_callback.second)),
      false /* check_error_state */, chromeos::ConnectCallbackMode::ON_STARTED);
}

void ArcNetHostImpl::StartDisconnect(const std::string& guid,
                                     StartDisconnectCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetNetworkConnectionHandler()->DisconnectNetwork(
      path,
      base::BindOnce(&StartDisconnectSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&StartDisconnectFailureCallback,
                     std::move(split_callback.second)));
}

void ArcNetHostImpl::GetWifiEnabledState(GetWifiEnabledStateCallback callback) {
  bool is_enabled = GetStateHandler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi());
  std::move(callback).Run(is_enabled);
}

void ArcNetHostImpl::SetWifiEnabledState(bool is_enabled,
                                         SetWifiEnabledStateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto state = GetStateHandler()->GetTechnologyState(
      chromeos::NetworkTypePattern::WiFi());
  // WiFi can't be enabled or disabled in these states.
  if ((state == chromeos::NetworkStateHandler::TECHNOLOGY_PROHIBITED) ||
      (state == chromeos::NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) ||
      (state == chromeos::NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)) {
    LOG(ERROR) << "SetWifiEnabledState failed due to WiFi state: " << state;
    std::move(callback).Run(false);
    return;
  }

  NET_LOG(USER) << __func__ << ":" << is_enabled;
  GetStateHandler()->SetTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi(), is_enabled,
      chromeos::network_handler::ErrorCallback());
  std::move(callback).Run(true);
}

void ArcNetHostImpl::StartScan() {
  GetStateHandler()->RequestScan(chromeos::NetworkTypePattern::WiFi());
}

void ArcNetHostImpl::ScanCompleted(const chromeos::DeviceState* /*unused*/) {
  auto* net_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(), ScanCompleted);
  if (!net_instance)
    return;

  net_instance->ScanCompleted();
}

void ArcNetHostImpl::DeviceListChanged() {
  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   WifiEnabledStateChanged);
  if (!net_instance)
    return;

  bool is_enabled = GetStateHandler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi());
  net_instance->WifiEnabledStateChanged(is_enabled);
}

std::string ArcNetHostImpl::LookupArcVpnServicePath() {
  chromeos::NetworkStateHandler::NetworkStateList state_list;
  GetStateHandler()->GetNetworkListByType(
      chromeos::NetworkTypePattern::VPN(), true /* configured_only */,
      false /* visible_only */, kGetNetworksListLimit, &state_list);

  for (const chromeos::NetworkState* state : state_list) {
    const auto* shill_backed_network = GetShillBackedNetwork(state);
    if (!shill_backed_network)
      continue;

    if (shill_backed_network->GetVpnProviderType() == shill::kProviderArcVpn)
      return shill_backed_network->path();
  }
  return std::string();
}

void ArcNetHostImpl::ConnectArcVpn(const std::string& service_path,
                                   const std::string& /* guid */) {
  arc_vpn_service_path_ = service_path;

  GetNetworkConnectionHandler()->ConnectToNetwork(
      service_path, base::BindOnce(&ArcVpnSuccessCallback),
      base::BindOnce(&ArcVpnErrorCallback, "connecting ARC VPN"),
      false /* check_error_state */,
      chromeos::ConnectCallbackMode::ON_COMPLETED);
}

std::unique_ptr<base::Value> ArcNetHostImpl::TranslateStringListToValue(
    const std::vector<std::string>& string_list) {
  auto result = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const auto& item : string_list)
    result->Append(item);
  return result;
}

std::unique_ptr<base::DictionaryValue>
ArcNetHostImpl::TranslateVpnConfigurationToOnc(
    const mojom::AndroidVpnConfiguration& cfg) {
  auto top_dict = std::make_unique<base::DictionaryValue>();

  // Name, Type
  top_dict->SetKey(
      onc::network_config::kName,
      base::Value(cfg.session_name.empty() ? cfg.app_label : cfg.session_name));
  top_dict->SetKey(onc::network_config::kType,
                   base::Value(onc::network_config::kVPN));

  // StaticIPConfig dictionary
  top_dict->SetKey(onc::network_config::kIPAddressConfigType,
                   base::Value(onc::network_config::kIPConfigTypeStatic));
  top_dict->SetKey(onc::network_config::kNameServersConfigType,
                   base::Value(onc::network_config::kIPConfigTypeStatic));

  std::unique_ptr<base::DictionaryValue> ip_dict =
      std::make_unique<base::DictionaryValue>();
  ip_dict->SetKey(onc::ipconfig::kType, base::Value(onc::ipconfig::kIPv4));
  ip_dict->SetKey(onc::ipconfig::kIPAddress, base::Value(cfg.ipv4_gateway));
  ip_dict->SetKey(onc::ipconfig::kRoutingPrefix, base::Value(32));
  ip_dict->SetKey(onc::ipconfig::kGateway, base::Value(cfg.ipv4_gateway));

  ip_dict->SetKey(onc::ipconfig::kNameServers,
                  base::Value::FromUniquePtrValue(
                      TranslateStringListToValue(cfg.nameservers)));
  ip_dict->SetKey(
      onc::ipconfig::kSearchDomains,
      base::Value::FromUniquePtrValue(TranslateStringListToValue(cfg.domains)));
  ip_dict->SetKey(onc::ipconfig::kIncludedRoutes,
                  base::Value::FromUniquePtrValue(
                      TranslateStringListToValue(cfg.split_include)));
  ip_dict->SetKey(onc::ipconfig::kExcludedRoutes,
                  base::Value::FromUniquePtrValue(
                      TranslateStringListToValue(cfg.split_exclude)));

  top_dict->SetKey(onc::network_config::kStaticIPConfig,
                   base::Value::FromUniquePtrValue(std::move(ip_dict)));

  // VPN dictionary
  std::unique_ptr<base::DictionaryValue> vpn_dict =
      std::make_unique<base::DictionaryValue>();
  vpn_dict->SetKey(onc::vpn::kHost, base::Value(cfg.app_name));
  vpn_dict->SetKey(onc::vpn::kType, base::Value(onc::vpn::kArcVpn));

  // ARCVPN dictionary
  std::unique_ptr<base::DictionaryValue> arcvpn_dict =
      std::make_unique<base::DictionaryValue>();
  arcvpn_dict->SetKey(
      onc::arc_vpn::kTunnelChrome,
      base::Value(cfg.tunnel_chrome_traffic ? "true" : "false"));
  vpn_dict->SetKey(onc::vpn::kArcVpn,
                   base::Value::FromUniquePtrValue(std::move(arcvpn_dict)));

  top_dict->SetKey(onc::network_config::kVPN,
                   base::Value::FromUniquePtrValue(std::move(vpn_dict)));

  return top_dict;
}

void ArcNetHostImpl::AndroidVpnConnected(
    mojom::AndroidVpnConfigurationPtr cfg) {
  auto properties = TranslateVpnConfigurationToOnc(*cfg);
  std::string service_path = LookupArcVpnServicePath();
  if (!service_path.empty()) {
    GetManagedConfigurationHandler()->SetProperties(
        service_path, *properties,
        base::BindOnce(&ArcNetHostImpl::ConnectArcVpn,
                       weak_factory_.GetWeakPtr(), service_path, std::string()),
        base::BindOnce(&ArcVpnErrorCallback,
                       "reconnecting ARC VPN " + service_path));
    return;
  }

  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::BindOnce(&ArcNetHostImpl::ConnectArcVpn,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ArcVpnErrorCallback, "connecting new ARC VPN"));
}

void ArcNetHostImpl::AndroidVpnStateChanged(mojom::ConnectionStateType state) {
  if (state != arc::mojom::ConnectionStateType::NOT_CONNECTED ||
      arc_vpn_service_path_.empty()) {
    return;
  }

  // DisconnectNetwork() invokes DisconnectRequested() through the
  // observer interface, so make sure it doesn't generate an unwanted
  // mojo call to Android.
  std::string service_path(arc_vpn_service_path_);
  arc_vpn_service_path_.clear();

  GetNetworkConnectionHandler()->DisconnectNetwork(
      service_path, base::BindOnce(&ArcVpnSuccessCallback),
      base::BindOnce(&ArcVpnErrorCallback, "disconnecting ARC VPN"));
}

void ArcNetHostImpl::SetAlwaysOnVpn(const std::string& vpn_package,
                                    bool lockdown) {
  // pref_service_ should be set by ArcServiceLauncher.
  DCHECK(pref_service_);
  pref_service_->SetString(prefs::kAlwaysOnVpnPackage, vpn_package);
  pref_service_->SetBoolean(prefs::kAlwaysOnVpnLockdown, lockdown);
}

void ArcNetHostImpl::DisconnectArcVpn() {
  arc_vpn_service_path_.clear();

  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   DisconnectAndroidVpn);
  if (!net_instance)
    return;

  net_instance->DisconnectAndroidVpn();
}

void ArcNetHostImpl::DisconnectRequested(const std::string& service_path) {
  if (arc_vpn_service_path_ != service_path) {
    return;
  }

  // This code path is taken when a user clicks the blue Disconnect button
  // in Chrome OS.  Chrome is about to send the Disconnect call to shill,
  // so update our local state and tell Android to disconnect the VPN.
  DisconnectArcVpn();
}

void ArcNetHostImpl::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  const auto* shill_backed_network = GetShillBackedNetwork(network);
  if (!shill_backed_network)
    return;

  if (arc_vpn_service_path_ != shill_backed_network->path() ||
      shill_backed_network->IsConnectingOrConnected())
    return;

  // This code path is taken when shill disconnects the Android VPN
  // service.  This can happen if a user tries to connect to a Chrome OS
  // VPN, and shill's VPNProvider::DisconnectAll() forcibly disconnects
  // all other VPN services to avoid a conflict.
  DisconnectArcVpn();
}

void ArcNetHostImpl::NetworkPropertiesUpdated(
    const chromeos::NetworkState* network) {
  if (!IsActiveNetworkState(network))
    return;

  chromeos::NetworkHandler::Get()
      ->network_configuration_handler()
      ->GetShillProperties(
          network->path(),
          base::BindOnce(&ArcNetHostImpl::ReceiveShillProperties,
                         weak_factory_.GetWeakPtr()));
}

void ArcNetHostImpl::ReceiveShillProperties(
    const std::string& service_path,
    absl::optional<base::Value> shill_properties) {
  if (!shill_properties) {
    LOG(ERROR) << "Failed to get shill Service properties for " << service_path;
    return;
  }

  // Ignore properties received after the network has disconnected.
  const auto* network = GetStateHandler()->GetNetworkState(service_path);
  if (!IsActiveNetworkState(network))
    return;

  base::InsertOrAssign(shill_network_properties_, service_path,
                       std::move(*shill_properties));
  UpdateActiveNetworks();
}

void ArcNetHostImpl::UpdateActiveNetworks() {
  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   ActiveNetworksChanged);
  if (!net_instance)
    return;

  net_instance->ActiveNetworksChanged(TranslateNetworkStates(
      arc_vpn_service_path_, GetActiveNetworks(), shill_network_properties_));
}

void ArcNetHostImpl::NetworkListChanged() {
  // Forget properties of disconnected networks
  base::EraseIf(shill_network_properties_, [](const auto& entry) {
    return !IsActiveNetworkState(
        GetStateHandler()->GetNetworkState(entry.first));
  });
  const auto active_networks = GetActiveNetworks();
  // If there is no active networks, send an explicit ActiveNetworksChanged
  // event to ARC and skip updating Shill properties.
  if (active_networks.empty()) {
    UpdateActiveNetworks();
    return;
  }
  for (const auto* network : active_networks)
    NetworkPropertiesUpdated(network);
}

void ArcNetHostImpl::OnShuttingDown() {
  DCHECK(observing_network_state_);
  GetStateHandler()->RemoveObserver(this, FROM_HERE);
  GetNetworkConnectionHandler()->RemoveObserver(this);
  observing_network_state_ = false;
}

}  // namespace arc
