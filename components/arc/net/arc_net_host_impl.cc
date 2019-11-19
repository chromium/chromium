// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/arc_net_host_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
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

bool IsDeviceOwner() {
  // Check whether the logged-in Chrome OS user is allowed to add or remove WiFi
  // networks. The user account state changes immediately after boot. There is a
  // small window when this may return an incorrect state. However, after things
  // settle down this is guranteed to reflect the correct user account state.
  return user_manager::UserManager::Get()->GetActiveUser()->GetAccountId() ==
         user_manager::UserManager::Get()->GetOwnerAccountId();
}

std::string GetStringFromONCDictionary(const base::Value* dict,
                                       const char* key,
                                       bool required) {
  DCHECK(dict->is_dict());
  const base::Value* string_value =
      dict->FindKeyOfType(key, base::Value::Type::STRING);
  if (!string_value) {
    LOG_IF(ERROR, required) << "Required property " << key << " not found.";
    return std::string();
  }
  std::string result = string_value->GetString();
  LOG_IF(ERROR, required && result.empty())
      << "Required property " << key << " is empty.";
  return result;
}

arc::mojom::SecurityType TranslateONCWifiSecurityType(
    const base::DictionaryValue* dict) {
  std::string type = GetStringFromONCDictionary(dict, onc::wifi::kSecurity,
                                                true /* required */);
  if (type == onc::wifi::kWEP_PSK)
    return arc::mojom::SecurityType::WEP_PSK;
  if (type == onc::wifi::kWEP_8021X)
    return arc::mojom::SecurityType::WEP_8021X;
  if (type == onc::wifi::kWPA_PSK)
    return arc::mojom::SecurityType::WPA_PSK;
  if (type == onc::wifi::kWPA_EAP)
    return arc::mojom::SecurityType::WPA_EAP;
  return arc::mojom::SecurityType::NONE;
}

arc::mojom::TetheringClientState TranslateTetheringState(
    const std::string& tethering_state) {
  if (tethering_state == onc::tethering_state::kTetheringConfirmedState)
    return arc::mojom::TetheringClientState::CONFIRMED;
  else if (tethering_state == onc::tethering_state::kTetheringNotDetectedState)
    return arc::mojom::TetheringClientState::NOT_DETECTED;
  else if (tethering_state == onc::tethering_state::kTetheringSuspectedState)
    return arc::mojom::TetheringClientState::SUSPECTED;
  NOTREACHED() << "Invalid tethering state: " << tethering_state;
  return arc::mojom::TetheringClientState::NOT_DETECTED;
}

arc::mojom::WiFiPtr TranslateONCWifi(const base::DictionaryValue* dict) {
  arc::mojom::WiFiPtr wifi = arc::mojom::WiFi::New();

  // Optional; defaults to 0.
  dict->GetInteger(onc::wifi::kFrequency, &wifi->frequency);

  wifi->bssid =
      GetStringFromONCDictionary(dict, onc::wifi::kBSSID, false /* required */);
  wifi->hex_ssid = GetStringFromONCDictionary(dict, onc::wifi::kHexSSID,
                                              true /* required */);

  // Optional; defaults to false.
  dict->GetBoolean(onc::wifi::kHiddenSSID, &wifi->hidden_ssid);

  wifi->security = TranslateONCWifiSecurityType(dict);

  // Optional; defaults to 0.
  dict->GetInteger(onc::wifi::kSignalStrength, &wifi->signal_strength);

  return wifi;
}

// Extracts WiFi's tethering client state from a dictionary of WiFi properties.
arc::mojom::TetheringClientState GetWifiTetheringClientState(
    const base::DictionaryValue* dict) {
  std::string tethering_state;
  dict->GetString(onc::wifi::kTetheringState, &tethering_state);
  return TranslateTetheringState(tethering_state);
}

arc::mojom::IPConfigurationPtr TranslateONCIPConfig(
    const base::Value* ip_dict) {
  DCHECK(ip_dict->is_dict());

  arc::mojom::IPConfigurationPtr configuration =
      arc::mojom::IPConfiguration::New();

  const base::Value* ip_address = ip_dict->FindKeyOfType(
      onc::ipconfig::kIPAddress, base::Value::Type::STRING);
  if (ip_address && !ip_address->GetString().empty()) {
    configuration->ip_address = ip_address->GetString();
    const base::Value* routing_prefix = ip_dict->FindKeyOfType(
        onc::ipconfig::kRoutingPrefix, base::Value::Type::INTEGER);
    if (routing_prefix)
      configuration->routing_prefix = routing_prefix->GetInt();
    else
      LOG(ERROR) << "Required property RoutingPrefix not found.";
    configuration->gateway = GetStringFromONCDictionary(
        ip_dict, onc::ipconfig::kGateway, true /* required */);
  }

  const base::Value* name_servers = ip_dict->FindKeyOfType(
      onc::ipconfig::kNameServers, base::Value::Type::LIST);
  if (name_servers) {
    for (const auto& entry : name_servers->GetList())
      configuration->name_servers.push_back(entry.GetString());
  }

  const base::Value* type =
      ip_dict->FindKeyOfType(onc::ipconfig::kType, base::Value::Type::STRING);
  configuration->type = type && type->GetString() == onc::ipconfig::kIPv6
                            ? arc::mojom::IPAddressType::IPV6
                            : arc::mojom::IPAddressType::IPV4;

  configuration->web_proxy_auto_discovery_url = GetStringFromONCDictionary(
      ip_dict, onc::ipconfig::kWebProxyAutoDiscoveryUrl, false /* required */);

  return configuration;
}

// Returns an IPConfiguration vector from the IPConfigs ONC property, which may
// include multiple IP configurations (e.g. IPv4 and IPv6).
std::vector<arc::mojom::IPConfigurationPtr> IPConfigurationsFromONCIPConfigs(
    const base::Value* dict) {
  const base::Value* ip_config_list =
      dict->FindKey(onc::network_config::kIPConfigs);
  if (!ip_config_list || !ip_config_list->is_list())
    return {};
  std::vector<arc::mojom::IPConfigurationPtr> result;
  for (const auto& entry : ip_config_list->GetList()) {
    arc::mojom::IPConfigurationPtr config = TranslateONCIPConfig(&entry);
    if (config)
      result.push_back(std::move(config));
  }
  return result;
}

// Returns an IPConfiguration vector from ONC property |property|, which will
// include a single IP configuration.
std::vector<arc::mojom::IPConfigurationPtr> IPConfigurationsFromONCProperty(
    const base::Value* dict,
    const char* property_key) {
  const base::Value* ip_dict = dict->FindKey(property_key);
  if (!ip_dict)
    return {};
  arc::mojom::IPConfigurationPtr config = TranslateONCIPConfig(ip_dict);
  if (!config)
    return {};
  std::vector<arc::mojom::IPConfigurationPtr> result;
  result.push_back(std::move(config));
  return result;
}

arc::mojom::ConnectionStateType TranslateONCConnectionState(
    const base::DictionaryValue* dict) {
  std::string connection_state = GetStringFromONCDictionary(
      dict, onc::network_config::kConnectionState, false /* required */);

  if (connection_state == onc::connection_state::kConnected)
    return arc::mojom::ConnectionStateType::CONNECTED;
  if (connection_state == onc::connection_state::kConnecting)
    return arc::mojom::ConnectionStateType::CONNECTING;
  return arc::mojom::ConnectionStateType::NOT_CONNECTED;
}

// Translates a shill connection state into a mojo ConnectionStateType.
// This is effectively the inverse function of shill.Service::GetStateString
// defined in platform2/shill/service.cc, with in addition some of shill's
// connection states translated to the same ConnectionStateType value.
arc::mojom::ConnectionStateType TranslateConnectionState(
    const std::string& state) {
  if (state == shill::kStateReady)
    return arc::mojom::ConnectionStateType::CONNECTED;
  if (state == shill::kStateAssociation ||
      state == shill::kStateConfiguration)
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

void TranslateONCNetworkTypeDetails(const base::DictionaryValue* dict,
                                    arc::mojom::NetworkConfiguration* mojo) {
  std::string type = GetStringFromONCDictionary(
      dict, onc::network_config::kType, true /* required */);
  // This property will be updated as required by the relevant network types
  // below.
  mojo->tethering_client_state = arc::mojom::TetheringClientState::NOT_DETECTED;
  if (type == onc::network_type::kCellular) {
    mojo->type = arc::mojom::NetworkType::CELLULAR;
  } else if (type == onc::network_type::kEthernet) {
    mojo->type = arc::mojom::NetworkType::ETHERNET;
  } else if (type == onc::network_type::kVPN) {
    mojo->type = arc::mojom::NetworkType::VPN;
  } else if (type == onc::network_type::kWiFi) {
    mojo->type = arc::mojom::NetworkType::WIFI;
    const base::DictionaryValue* wifi_dict = nullptr;
    dict->GetDictionary(onc::network_config::kWiFi, &wifi_dict);
    DCHECK(wifi_dict);
    mojo->wifi = TranslateONCWifi(wifi_dict);
    mojo->tethering_client_state = GetWifiTetheringClientState(wifi_dict);
  } else {
    NOTREACHED() << "Unknown network type: " << type;
  }
}

// Add shill's Device properties to the given mojo NetworkConfiguration objects.
// This adds the network interface and current IP configurations.
void AddDeviceProperties(arc::mojom::NetworkConfiguration* network,
                         const std::string& device_path) {
  const auto* device = GetStateHandler()->GetDeviceState(device_path);
  if (!device)
    return;

  network->network_interface = device->interface();

  // IP configurations were already obtained through cached ONC properties.
  if (network->ip_configs)
    return;

  std::vector<arc::mojom::IPConfigurationPtr> ip_configs;
  for (const auto& kv : device->ip_configs()) {
    auto ip_config = arc::mojom::IPConfiguration::New();
    if (const std::string* r =
            kv.second->FindStringPath(shill::kAddressProperty))
      ip_config->ip_address = *r;
    if (const std::string* r =
            kv.second->FindStringPath(shill::kGatewayProperty))
      ip_config->gateway = *r;
    ip_config->routing_prefix =
        kv.second->FindIntPath(shill::kPrefixlenProperty).value_or(0);
    ip_config->type = (ip_config->routing_prefix < 64)
                          ? arc::mojom::IPAddressType::IPV4
                          : arc::mojom::IPAddressType::IPV6;
    if (const base::Value* dns_list =
            kv.second->FindListKey(shill::kNameServersProperty)) {
      for (const auto& dnsValue : dns_list->GetList()) {
        const std::string& dns = dnsValue.GetString();
        if (dns.empty())
          continue;

        // When manually setting DNS, up to 4 addresses can be specified in the
        // UI. Unspecified entries can show up as 0.0.0.0 and should be removed.
        if (dns == "0.0.0.0")
          continue;

        ip_config->name_servers.push_back(dns);
      }
    }
    ip_configs.push_back(std::move(ip_config));
  }

  network->ip_configs = std::move(ip_configs);
}

arc::mojom::NetworkConfigurationPtr TranslateONCConfiguration(
    const chromeos::NetworkState* network_state,
    const base::DictionaryValue* dict) {
  arc::mojom::NetworkConfigurationPtr mojo =
      arc::mojom::NetworkConfiguration::New();

  mojo->connection_state = TranslateONCConnectionState(dict);

  mojo->guid = GetStringFromONCDictionary(dict, onc::network_config::kGUID,
                                          true /* required */);

  // crbug.com/761708 - VPNs do not currently have an IPConfigs array,
  // so in order to fetch the parameters (particularly the DNS server list),
  // fall back to StaticIPConfig or SavedIPConfig.
  std::vector<arc::mojom::IPConfigurationPtr> ip_configs =
      IPConfigurationsFromONCIPConfigs(dict);
  if (ip_configs.empty()) {
    ip_configs = IPConfigurationsFromONCProperty(
        dict, onc::network_config::kStaticIPConfig);
  }
  if (ip_configs.empty()) {
    ip_configs = IPConfigurationsFromONCProperty(
        dict, onc::network_config::kSavedIPConfig);
  }
  if (!ip_configs.empty())
    mojo->ip_configs = std::move(ip_configs);

  mojo->guid = GetStringFromONCDictionary(dict, onc::network_config::kGUID,
                                          true /* required */);
  mojo->mac_address = GetStringFromONCDictionary(
      dict, onc::network_config::kMacAddress, false /* required */);
  TranslateONCNetworkTypeDetails(dict, mojo.get());

  if (network_state) {
    mojo->connection_state =
        TranslateConnectionState(network_state->connection_state());
    AddDeviceProperties(mojo.get(), network_state->device_path());
  }

  return mojo;
}

const chromeos::NetworkState* GetShillBackedNetwork(
    const chromeos::NetworkState* network) {
  if (!network)
    return nullptr;

  // Non-Tether networks are already backed by Shill.
  if (!chromeos::NetworkTypePattern::Tether().MatchesType(network->type()))
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
    const chromeos::NetworkStateHandler::NetworkStateList& network_states) {
  std::vector<arc::mojom::NetworkConfigurationPtr> networks;
  for (const chromeos::NetworkState* state : network_states) {
    const std::string& network_path = state->path();
    if (network_path == arc_vpn_path) {
      // Never tell Android about its own VPN.
      continue;
    }
    // For tethered networks, the underlying WiFi networks are not part of
    // active networks. Replace any such tethered network with its underlying
    // backing network, because ARC cannot match its datapath with the tethered
    // network configuration.
    state = GetShillBackedNetwork(state);
    if (!state) {
      continue;
    }
    auto network = TranslateONCConfiguration(
        state, chromeos::network_util::TranslateNetworkStateToONC(state).get());
    network->is_default_network =
        (network_path == GetStateHandler()->default_network_path());
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
  VLOG(1) << "ForgetNetworkFailureCallback: " << error_name;
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
  VLOG(1) << "StartConnectFailureCallback: " << error_name;
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
  VLOG(1) << "StartDisconnectFailureCallback: " << error_name;
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void GetDefaultNetworkSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkConfigurationPtr,
                            arc::mojom::NetworkConfigurationPtr)> callback,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  // TODO(cernekee): Figure out how to query Chrome for the default physical
  // service if a VPN is connected, rather than just reporting the
  // default logical service in both fields.
  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkState(service_path);
  std::move(callback).Run(
      TranslateONCConfiguration(network_state, &dictionary),
      TranslateONCConfiguration(network_state, &dictionary));
}

void GetDefaultNetworkFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkConfigurationPtr,
                            arc::mojom::NetworkConfigurationPtr)> callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "Failed to query default logical network: " << error_name;
  std::move(callback).Run(nullptr, nullptr);
}

void DefaultNetworkFailureCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "Failed to query default logical network: " << error_name;
}

void ArcVpnSuccessCallback() {
  DVLOG(1) << "ArcVpnSuccessCallback";
}

void ArcVpnErrorCallback(const std::string& error_name,
                         std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "ArcVpnErrorCallback: " << error_name;
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
    VLOG(0) << "Disconnecting stale ARC VPN " << default_network->path();
    GetNetworkConnectionHandler()->DisconnectNetwork(
        default_network->path(), base::Bind(&ArcVpnSuccessCallback),
        base::Bind(&ArcVpnErrorCallback));
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

  std::vector<mojom::NetworkConfigurationPtr> networks =
      TranslateNetworkStates(arc_vpn_service_path_, network_states);
  std::move(callback).Run(mojom::GetNetworksResponseType::New(
      arc::mojom::NetworkResult::SUCCESS, std::move(networks)));
}

void ArcNetHostImpl::CreateNetworkSuccessCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& service_path,
    const std::string& guid) {
  VLOG(1) << "CreateNetworkSuccessCallback";

  cached_guid_ = guid;
  cached_service_path_ = service_path;

  std::move(callback).Run(guid);
}

void ArcNetHostImpl::CreateNetworkFailureCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  VLOG(1) << "CreateNetworkFailureCallback: " << error_name;
  std::move(callback).Run(std::string());
}

void ArcNetHostImpl::CreateNetwork(mojom::WifiConfigurationPtr cfg,
                                   CreateNetworkCallback callback) {
  if (!IsDeviceOwner()) {
    std::move(callback).Run(std::string());
    return;
  }

  std::unique_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> wifi_dict(new base::DictionaryValue);

  if (!cfg->hexssid.has_value() || !cfg->details) {
    std::move(callback).Run(std::string());
    return;
  }
  mojom::ConfiguredNetworkDetailsPtr details =
      std::move(cfg->details->get_configured());
  if (!details) {
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
  properties->SetWithoutPathExpansion(onc::network_config::kWiFi,
                                      std::move(wifi_dict));

  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::Bind(&ArcNetHostImpl::CreateNetworkSuccessCallback,
                 weak_factory_.GetWeakPtr(), repeating_callback),
      base::Bind(&ArcNetHostImpl::CreateNetworkFailureCallback,
                 weak_factory_.GetWeakPtr(), repeating_callback));
}

bool ArcNetHostImpl::GetNetworkPathFromGuid(const std::string& guid,
                                            std::string* path) {
  const chromeos::NetworkState* network =
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
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  cached_guid_.clear();
  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetManagedConfigurationHandler()->RemoveConfiguration(
      path, base::Bind(&ForgetNetworkSuccessCallback, repeating_callback),
      base::Bind(&ForgetNetworkFailureCallback, repeating_callback));
}

void ArcNetHostImpl::StartConnect(const std::string& guid,
                                  StartConnectCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetNetworkConnectionHandler()->ConnectToNetwork(
      path, base::Bind(&StartConnectSuccessCallback, repeating_callback),
      base::Bind(&StartConnectFailureCallback, repeating_callback),
      false /* check_error_state */, chromeos::ConnectCallbackMode::ON_STARTED);
}

void ArcNetHostImpl::StartDisconnect(const std::string& guid,
                                     StartDisconnectCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetNetworkConnectionHandler()->DisconnectNetwork(
      path, base::Bind(&StartDisconnectSuccessCallback, repeating_callback),
      base::Bind(&StartDisconnectFailureCallback, repeating_callback));
}

void ArcNetHostImpl::GetWifiEnabledState(GetWifiEnabledStateCallback callback) {
  bool is_enabled = GetStateHandler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi());
  std::move(callback).Run(is_enabled);
}

void ArcNetHostImpl::SetWifiEnabledState(bool is_enabled,
                                         SetWifiEnabledStateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  chromeos::NetworkStateHandler::TechnologyState state =
      GetStateHandler()->GetTechnologyState(
          chromeos::NetworkTypePattern::WiFi());
  // WiFi can't be enabled or disabled in these states.
  if ((state == chromeos::NetworkStateHandler::TECHNOLOGY_PROHIBITED) ||
      (state == chromeos::NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) ||
      (state == chromeos::NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)) {
    VLOG(1) << "SetWifiEnabledState failed due to WiFi state: " << state;
    std::move(callback).Run(false);
    return;
  }
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

const chromeos::NetworkState* ArcNetHostImpl::GetDefaultNetworkFromChrome() {
  // If an Android VPN is connected, report the underlying physical
  // connection only.  Never tell Android about its own VPN.
  // If a Chrome OS VPN is connected, report the Chrome OS VPN as the
  // default connection.
  if (arc_vpn_service_path_.empty()) {
    return GetShillBackedNetwork(GetStateHandler()->DefaultNetwork());
  }

  return GetShillBackedNetwork(GetStateHandler()->ConnectedNetworkByType(
      chromeos::NetworkTypePattern::NonVirtual()));
}

void ArcNetHostImpl::GetDefaultNetwork(GetDefaultNetworkCallback callback) {
  const chromeos::NetworkState* default_network = GetDefaultNetworkFromChrome();

  if (!default_network) {
    VLOG(1) << "GetDefaultNetwork: no default network";
    std::move(callback).Run(nullptr, nullptr);
    return;
  }
  VLOG(1) << "GetDefaultNetwork: default network is "
          << default_network->path();
  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, default_network->path(),
      base::Bind(&GetDefaultNetworkSuccessCallback, repeating_callback),
      base::Bind(&GetDefaultNetworkFailureCallback, repeating_callback));
}

void ArcNetHostImpl::DefaultNetworkSuccessCallback(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   DefaultNetworkChanged);
  if (!net_instance)
    return;

  const chromeos::NetworkState* network_state =
      GetStateHandler()->GetNetworkState(service_path);
  net_instance->DefaultNetworkChanged(
      TranslateONCConfiguration(network_state, &dictionary),
      TranslateONCConfiguration(network_state, &dictionary));
}

void ArcNetHostImpl::UpdateDefaultNetwork() {
  const chromeos::NetworkState* default_network = GetDefaultNetworkFromChrome();

  if (!default_network) {
    VLOG(1) << "No default network";
    auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                     DefaultNetworkChanged);
    if (net_instance)
      net_instance->DefaultNetworkChanged(nullptr, nullptr);
    return;
  }

  VLOG(1) << "New default network: " << default_network->path() << " ("
          << default_network->type() << ")";
  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, default_network->path(),
      base::Bind(&ArcNetHostImpl::DefaultNetworkSuccessCallback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DefaultNetworkFailureCallback));
}

void ArcNetHostImpl::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  UpdateDefaultNetwork();

  // If the the default network switched between two networks, also send an
  // ActiveNetworkChanged notification to let ARC observe the switch.
  chromeos::NetworkStateHandler::NetworkStateList network_states;
  GetStateHandler()->GetActiveNetworkListByType(
      chromeos::NetworkTypePattern::Default(), &network_states);
  if (network_states.size() > 1) {
    ActiveNetworksChanged(network_states);
  }
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
    const chromeos::NetworkState* shill_backed_network =
        GetShillBackedNetwork(state);
    if (!shill_backed_network)
      continue;
    if (shill_backed_network->GetVpnProviderType() == shill::kProviderArcVpn) {
      return shill_backed_network->path();
    }
  }
  return std::string();
}

void ArcNetHostImpl::ConnectArcVpn(const std::string& service_path,
                                   const std::string& /* guid */) {
  DVLOG(1) << "ConnectArcVpn " << service_path;
  arc_vpn_service_path_ = service_path;

  GetNetworkConnectionHandler()->ConnectToNetwork(
      service_path, base::Bind(&ArcVpnSuccessCallback),
      base::Bind(&ArcVpnErrorCallback), false /* check_error_state */,
      chromeos::ConnectCallbackMode::ON_COMPLETED);
}

std::unique_ptr<base::Value> ArcNetHostImpl::TranslateStringListToValue(
    const std::vector<std::string>& string_list) {
  std::unique_ptr<base::Value> result =
      std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const auto& item : string_list) {
    result->Append(item);
  }
  return result;
}

std::unique_ptr<base::DictionaryValue>
ArcNetHostImpl::TranslateVpnConfigurationToOnc(
    const mojom::AndroidVpnConfiguration& cfg) {
  std::unique_ptr<base::DictionaryValue> top_dict =
      std::make_unique<base::DictionaryValue>();

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

  ip_dict->SetWithoutPathExpansion(onc::ipconfig::kNameServers,
                                   TranslateStringListToValue(cfg.nameservers));
  ip_dict->SetWithoutPathExpansion(onc::ipconfig::kSearchDomains,
                                   TranslateStringListToValue(cfg.domains));
  ip_dict->SetWithoutPathExpansion(
      onc::ipconfig::kIncludedRoutes,
      TranslateStringListToValue(cfg.split_include));
  ip_dict->SetWithoutPathExpansion(
      onc::ipconfig::kExcludedRoutes,
      TranslateStringListToValue(cfg.split_exclude));

  top_dict->SetWithoutPathExpansion(onc::network_config::kStaticIPConfig,
                                    std::move(ip_dict));

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
  vpn_dict->SetWithoutPathExpansion(onc::vpn::kArcVpn, std::move(arcvpn_dict));

  top_dict->SetWithoutPathExpansion(onc::network_config::kVPN,
                                    std::move(vpn_dict));

  return top_dict;
}

void ArcNetHostImpl::AndroidVpnConnected(
    mojom::AndroidVpnConfigurationPtr cfg) {
  std::unique_ptr<base::DictionaryValue> properties =
      TranslateVpnConfigurationToOnc(*cfg);

  if (!base::FeatureList::IsEnabled(arc::kVpnFeature)) {
    VLOG(1) << "AndroidVpnConnected: feature is disabled; ignoring";
    return;
  }

  std::string service_path = LookupArcVpnServicePath();
  if (!service_path.empty()) {
    VLOG(1) << "AndroidVpnConnected: reusing " << service_path;
    GetManagedConfigurationHandler()->SetProperties(
        service_path, *properties,
        base::Bind(&ArcNetHostImpl::ConnectArcVpn, weak_factory_.GetWeakPtr(),
                   service_path, std::string()),
        base::Bind(&ArcVpnErrorCallback));
    return;
  }

  VLOG(1) << "AndroidVpnConnected: creating new ARC VPN";
  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::Bind(&ArcNetHostImpl::ConnectArcVpn, weak_factory_.GetWeakPtr()),
      base::Bind(&ArcVpnErrorCallback));
}

void ArcNetHostImpl::AndroidVpnStateChanged(mojom::ConnectionStateType state) {
  VLOG(1) << "AndroidVpnStateChanged: state=" << state
          << " service=" << arc_vpn_service_path_;

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
      service_path, base::Bind(&ArcVpnSuccessCallback),
      base::Bind(&ArcVpnErrorCallback));
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
  if (!net_instance) {
    LOG(ERROR) << "User requested VPN disconnection but API is unavailable";
    return;
  }
  net_instance->DisconnectAndroidVpn();
}

void ArcNetHostImpl::DisconnectRequested(const std::string& service_path) {
  if (arc_vpn_service_path_ != service_path) {
    return;
  }

  // This code path is taken when a user clicks the blue Disconnect button
  // in Chrome OS.  Chrome is about to send the Disconnect call to shill,
  // so update our local state and tell Android to disconnect the VPN.
  VLOG(1) << "DisconnectRequested " << service_path;
  DisconnectArcVpn();
}

void ArcNetHostImpl::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  // DefaultNetworkChanged() won't be invoked if an ARC VPN is the default
  // network and the underlying physical connection changed, so check for
  // that condition here.  This is invoked any time any service state
  // changes.
  UpdateDefaultNetwork();

  const chromeos::NetworkState* shill_backed_network =
      GetShillBackedNetwork(network);
  if (!shill_backed_network)
    return;

  if (arc_vpn_service_path_ != shill_backed_network->path() ||
      shill_backed_network->IsConnectingOrConnected()) {
    return;
  }

  // This code path is taken when shill disconnects the Android VPN
  // service.  This can happen if a user tries to connect to a Chrome OS
  // VPN, and shill's VPNProvider::DisconnectAll() forcibly disconnects
  // all other VPN services to avoid a conflict.
  VLOG(1) << "NetworkConnectionStateChanged " << shill_backed_network->path();
  DisconnectArcVpn();
}

void ArcNetHostImpl::ActiveNetworksChanged(
    const std::vector<const chromeos::NetworkState*>& active_networks) {
  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   ActiveNetworksChanged);
  if (!net_instance)
    return;

  std::vector<arc::mojom::NetworkConfigurationPtr> network_configurations =
      TranslateNetworkStates(arc_vpn_service_path_, active_networks);
  net_instance->ActiveNetworksChanged(std::move(network_configurations));
}

void ArcNetHostImpl::NetworkListChanged() {
  // This is invoked any time the list of services is reordered or changed.
  // During the transition when a new service comes online, it will
  // temporarily be ranked below "inferior" services.  This callback
  // informs us that shill's ordering has been updated.
  UpdateDefaultNetwork();
}

void ArcNetHostImpl::OnShuttingDown() {
  DCHECK(observing_network_state_);
  GetStateHandler()->RemoveObserver(this, FROM_HERE);
  GetNetworkConnectionHandler()->RemoveObserver(this);
  observing_network_state_ = false;
}

}  // namespace arc
