// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/managed_state.h"
#include "chromeos/ash/components/network/network_config.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "url/gurl.h"

namespace ash {

class DeviceState;
class NetworkStateHandler;
class NetworkStateTest;

// Simple class to provide network state information about a network service.
// This class should always be passed as a const* and should never be held
// on to. Store network_state->path() (defined in ManagedState) instead and
// call NetworkStateHandler::GetNetworkState(path) to retrieve the state for
// the network.
//
// Note: NetworkStateHandler will store an entry for each member of
// Manager.ServiceCompleteList. The visible() method indicates whether the
// network is visible, and the IsInProfile() method indicates whether the
// network is saved in a profile.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkState : public ManagedState {
 public:
  explicit NetworkState(const std::string& path);

  NetworkState(const NetworkState&) = delete;
  NetworkState& operator=(const NetworkState&) = delete;

  ~NetworkState() override;

  struct VpnProviderInfo {
    // The id used by chrome to identify the provider (i.e. an extension id).
    std::string id;
    // The VPN type, provided by the VPN provider/extension.
    std::string type;
  };

  // This is reflected by network/enums.xml:NetworkPortalState.
  enum class PortalState {
    // The network is not connected or the portal state is not available.
    kUnknown,
    // The network is connected and no portal is detected.
    kOnline,
    // A portal is suspected but no redirect was provided.
    // TODO(b/336931625): Remove the kPortalSuspected field.
    kPortalSuspected,
    // The network is in a portal state with a redirect URL.
    kPortal,
    // The network is connected but no internet is available and no proxy was
    // detected.
    kNoInternet,
    kMaxValue = kNoInternet  // For UMA_HISTOGRAM_ENUMERATION
  };

  friend std::ostream& operator<<(std::ostream& stream, PortalState state);

  // ManagedState overrides
  // If you change this method, update GetProperties too.
  bool PropertyChanged(const std::string& key,
                       const base::Value& value) override;
  bool InitialPropertiesReceived(const base::Value::Dict& properties) override;
  void GetStateProperties(base::Value::Dict* dictionary) const override;
  bool IsActive() const override;

  // Called when the IPConfig properties may have changed. |properties| is
  // expected to be of type DICTIONARY.
  void IPConfigPropertiesChanged(const base::Value::Dict& properties);

  // Returns true if the network requires a service activation.
  bool RequiresActivation() const;

  // Returns true if the network security type requires a passphrase only.
  bool SecurityRequiresPassphraseOnly() const;

  // Accessors
  bool visible() const { return visible_; }
  void set_visible(bool visible) { visible_ = visible; }
  const std::string& security_class() const { return security_class_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& guid() const { return guid_; }
  const std::string& profile_path() const { return profile_path_; }
  const GURL& probe_url() const { return probe_url_; }
  ::onc::ONCSource onc_source() const { return onc_source_; }

  // Provides the error for the last attempt to connect/configure the network
  // (an empty string signifies no error at all). Note that this value can be
  // cleared - see ClearError() below.
  const std::string& GetError() const;

  // Clears the error associated with this network. Should be called whenever
  // a connection to this network is initiated or the associated configuration
  // is updated/removed.
  void ClearError();

  // Returns |connection_state_| if visible, kStateIdle otherwise.
  std::string connection_state() const;

  // Updates the connection state and saves the previous connection state.
  void SetConnectionState(const std::string& connection_state);

  int priority() const { return priority_; }

  const std::optional<base::Value::Dict>& proxy_config() const {
    return proxy_config_;
  }
  // TODO(b/340974631): Deprecate this getter and use network_config() instead.
  const std::optional<base::Value::Dict>& ipv4_config() const {
    return ipv4_config_;
  }
  // TODO(b/340974631): Deprecate this getter and use network_config() instead.
  std::string GetIpAddress() const;
  // TODO(b/340974631): Deprecate this getter and use network_config() instead.
  std::string GetGateway() const;
  GURL GetWebProxyAutoDiscoveryUrl() const;

  // Network service property accessors.
  // Link speeds are set when service is connected or link speeds get updated
  // during the connection. When link speeds are not set, std::nullopt is
  // returned.
  const std::optional<uint32_t> max_uplink_speed_kbps() const {
    return max_uplink_speed_kbps_;
  }
  const std::optional<uint32_t> max_downlink_speed_kbps() const {
    return max_downlink_speed_kbps_;
  }

  const NetworkConfig* network_config() const { return network_config_.get(); }

  // Wireless property accessors
  bool connectable() const { return connectable_; }
  void set_connectable(bool connectable) { connectable_ = connectable; }
  int signal_strength() const { return signal_strength_; }
  void set_signal_strength(int signal_strength) {
    signal_strength_ = signal_strength;
  }
  int16_t rssi() const { return rssi_; }
  void set_rssi(int16_t rssi) { rssi_ = rssi; }
  const std::string& bssid() const { return bssid_; }
  int frequency() const { return frequency_; }
  bool blocked_by_policy() const { return blocked_by_policy_; }
  void set_blocked_by_policy(bool blocked_by_policy) {
    blocked_by_policy_ = blocked_by_policy;
  }
  bool hidden_ssid() const { return hidden_ssid_; }
  const std::string& passpoint_id() const { return passpoint_id_; }
  bool metered() const { return metered_; }
  // Wifi property accessors
  const std::string& eap_method() const { return eap_method_; }
  const std::vector<uint8_t>& raw_ssid() const { return raw_ssid_; }

  // Cellular property accessors
  const std::string& eid() const { return eid_; }
  const std::string& iccid() const { return iccid_; }
  const std::string& network_technology() const { return network_technology_; }
  const std::string& activation_type() const { return activation_type_; }
  const std::string& activation_state() const { return activation_state_; }
  bool allow_roaming() const { return allow_roaming_; }
  const std::string& payment_method() const { return payment_method_; }
  const std::string& payment_url() const { return payment_url_; }
  const std::string& payment_post_data() const { return payment_post_data_; }
  bool cellular_out_of_credits() const { return cellular_out_of_credits_; }

  // VPN property accessors
  const VpnProviderInfo* vpn_provider() const { return vpn_provider_.get(); }
  std::string GetVpnProviderType() const;

  // Tether accessors and setters.
  int battery_percentage() const { return battery_percentage_; }
  void set_battery_percentage(int battery_percentage) {
    battery_percentage_ = battery_percentage;
  }
  const std::string& tether_carrier() const { return tether_carrier_; }
  void set_tether_carrier(const std::string& tether_carrier) {
    tether_carrier_ = tether_carrier;
  }
  bool tether_has_connected_to_host() const {
    return tether_has_connected_to_host_;
  }
  void set_tether_has_connected_to_host(bool tether_has_connected_to_host) {
    tether_has_connected_to_host_ = tether_has_connected_to_host;
  }
  const std::string& tether_guid() const { return tether_guid_; }
  void set_tether_guid(const std::string& guid) { tether_guid_ = guid; }

  bool connect_requested() const { return connect_requested_; }

  const std::string& shill_connect_error() const {
    return shill_connect_error_;
  }

  // Returns true if the network is managed by policy (determined by
  // |onc_source_|).
  bool IsManagedByPolicy() const;

  // Returns true if the network is roaming and the provider does not require
  // roaming.
  bool IndicateRoaming() const;

  // Returns true if the network security is WEP_8021x (Dynamic WEP)
  bool IsDynamicWep() const;

  // Returns true if |connection_state_| is a connected/connecting state.
  bool IsConnectedState() const;
  bool IsConnectingState() const;
  bool IsConnectingOrConnected() const;

  // Returns true if |connection_state_| is online.
  bool IsOnline() const;

  // Returns true if this is a network stored in a profile.
  bool IsInProfile() const;

  // Returns true if the network is never stored in a profile (e.g. Tether and
  // default Cellular).
  bool IsNonProfileType() const;

  // Returns true if the network properties are stored in a user profile.
  bool IsPrivate() const;

  // Returns true if the network is a Cellular network not backed by Shill
  // service.
  bool IsNonShillCellularNetwork() const;

  // Returns the state of automatic captive portal detection for the network.
  PortalState GetPortalState() const { return portal_state_; }

  // Returns true if the security type is non-empty and not 'none'.
  bool IsSecure() const;

  // Returns the |raw_ssid| as a hex-encoded string
  std::string GetHexSsid() const;

  // Returns a comma separated string of name servers.
  std::string GetDnsServersAsString() const;

  // Converts the prefix length to a netmask string.
  std::string GetNetmask() const;

  // Returns a specifier for identifying this network in the absence of a GUID.
  // This should only be used by NetworkStateHandler for keeping track of
  // GUIDs assigned to unsaved networks.
  std::string GetSpecifier() const;

  // Set the GUID. Called exclusively by NetworkStateHandler.
  void SetGuid(const std::string& guid);

  // Helpers for returning mojo types.
  chromeos::network_config::mojom::ActivationStateType GetMojoActivationState()
      const;
  chromeos::network_config::mojom::SecurityType GetMojoSecurity() const;

  // Helper for UMA stats. Corresponds to NetworkTechnology in enums.xml
  // which is also used by Shill metrics.
  enum class NetworkTechnologyType {
    kCellular = 0,
    kEthernet = 1,
    kEthernetEap = 2,
    kWiFi = 3,
    kTether = 4,
    kVPN = 5,
    kUnknown = 6,
    kMaxValue = kUnknown,
  };

  friend std::ostream& operator<<(std::ostream& stream,
                                  NetworkTechnologyType type);

  NetworkTechnologyType GetNetworkTechnologyType() const;

  // Setters for testing.
  void set_connection_state_for_testing(const std::string& connection_state) {
    connection_state_ = connection_state;
  }
  void set_connect_requested_for_testing(bool connect_requested) {
    connect_requested_ = connect_requested;
  }
  void set_network_technology_for_testing(const std::string& technology) {
    network_technology_ = technology;
  }

  // Helpers (used e.g. when a state, error, or shill dictionary is cached)
  static bool StateIsConnected(const std::string& connection_state);
  static bool StateIsConnecting(const std::string& connection_state);
  static bool StateIsPortalled(const std::string& connection_state);
  static bool ErrorIsValid(const std::string& error);
  static std::unique_ptr<NetworkState> CreateNonShillCellularNetwork(
      const std::string& iccid,
      const std::string& eid,
      const std::string& guid,
      bool is_managed,
      const std::string& cellular_device_path);

  // Ignore changes to signal strength less than this value.
  constexpr static const int kSignalStrengthChangeThreshold = 5;

 private:
  friend class MobileActivatorTest;
  friend class NetworkStateHandler;
  friend class NetworkStateTest;

  // Updates |name_| from the 'WiFi.HexSSID' entry in |properties|, if the key
  // exists, and validates |name_|. Returns true if |name_| changes.
  bool UpdateName(const base::Value::Dict& properties);

  // Uses the Shill connection state to generate |portal_state_|.
  void UpdateCaptivePortalState(const base::Value::Dict& properties);

  void SetVpnProvider(const std::string& id, const std::string& type);

  // Set to true if the network is a member of Manager.Services.
  bool visible_ = false;

  // Network Service properties. Avoid adding any additional properties here.
  // Instead use NetworkConfigurationHandler::GetProperties() to asynchronously
  // request properties from Shill.
  std::string security_class_;
  std::string eap_method_;    // Needed for WiFi EAP networks
  std::string eap_key_mgmt_;  // Needed for identifying Dynamic WEP networks
  std::string device_path_;
  std::string guid_;
  std::string tether_guid_;  // Used to double link a Tether and Wi-Fi network.
  std::string connection_state_ = shill::kStateIdle;
  std::string profile_path_;
  GURL probe_url_;
  std::vector<uint8_t> raw_ssid_;  // Unknown encoding. Not necessarily UTF-8.
  int priority_ = 0;  // kPriority, used for organizing known networks.
  ::onc::ONCSource onc_source_ = ::onc::ONC_SOURCE_UNKNOWN;
  std::optional<uint32_t> max_uplink_speed_kbps_;
  std::optional<uint32_t> max_downlink_speed_kbps_;
  std::unique_ptr<NetworkConfig> network_config_;

  // Last non empty Service.Error property. Expected to be cleared via
  // ClearError() when a connection attempt is initiated and when an associated
  // configuration is updated/removed.
  std::string last_error_;

  // The error message provided by the shill Service.Connect dbus method if the
  // most recent connect attempt failed. Otherwise empty.
  std::string shill_connect_error_;

  // Cached copy of the Shill Service IPConfig object. For ipv6 properties use
  // the ip_configs_ property in the corresponding DeviceState.
  std::optional<base::Value::Dict> ipv4_config_;

  // Wireless properties, used for icons and Connect logic.
  bool connectable_ = false;
  int signal_strength_ = 0;
  // Default RSSI value when it is unknown.
  // This value needs to be sync with shill::WiFiService::SignalLevelMin.
  int16_t rssi_ = std::numeric_limits<int16_t>::min();
  std::string bssid_;
  int frequency_ = 0;
  bool blocked_by_policy_ = false;
  bool hidden_ssid_ = false;
  std::string passpoint_id_;
  bool metered_ = false;

  // Cellular properties, used for icons, Connect, and Activation.
  std::string eid_;
  std::string iccid_;
  std::string network_technology_;
  std::string activation_type_;
  std::string activation_state_;
  std::string roaming_;
  bool allow_roaming_ = false;
  bool provider_requires_roaming_ = false;
  std::string payment_method_;
  std::string payment_url_;
  std::string payment_post_data_;
  bool cellular_out_of_credits_ = false;

  // VPN properties, used to construct the display name and to show the correct
  // configuration dialog. The id is the Extension ID or Arc package name for
  // extension or Arc provider VPNs.
  std::unique_ptr<VpnProviderInfo> vpn_provider_;

  // Tether properties.
  std::string tether_carrier_;
  int battery_percentage_ = 0;

  PortalState portal_state_ = PortalState::kUnknown;

  // Whether the current device has already connected to the tether host device
  // providing the hotspot corresponding to this NetworkState.
  // Note: this means that the current device has already connected to the
  // tether host, but it does not necessarily mean that the current device has
  // connected to the Tether network corresponding to this NetworkState.
  bool tether_has_connected_to_host_ = false;

  // TODO(pneubeck): Remove this once (Managed)NetworkConfigurationHandler
  // provides proxy configuration. crbug.com/241775
  std::optional<base::Value::Dict> proxy_config_;

  // Set while a network connect request is queued. Cleared on connect or
  // if the request is aborted.
  bool connect_requested_ = false;
};

COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::ostream& operator<<(std::ostream& stream,
                         NetworkState::PortalState portal_state);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::ostream& operator<<(std::ostream& stream,
                         NetworkState::NetworkTechnologyType type);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_STATE_H_
