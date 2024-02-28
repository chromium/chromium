// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_STATE_NOTIFIER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_STATE_NOTIFIER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkState;
class SystemTrayClient;

// This class provides user notifications in the following cases:
// 1. ShowNetworkConnectError() gets called after any user initiated connect
//    failure. This will handle displaying an error notification.
//    TODO(stevenjb): convert this class to use the new MessageCenter
//    notification system.
// 2. It observes NetworkState changes to generate notifications when a
//    Cellular network is out of credits.
// 3. Generates a notification when VPN is disconnected not as a result of
//    user's action.
class NetworkStateNotifier : public NetworkConnectionObserver,
                             public NetworkStateHandlerObserver {
 public:
  NetworkStateNotifier();

  NetworkStateNotifier(const NetworkStateNotifier&) = delete;
  NetworkStateNotifier& operator=(const NetworkStateNotifier&) = delete;

  ~NetworkStateNotifier() override;

  // Show a connection error notification. If |error_name| matches an error
  // defined in NetworkConnectionHandler for connect, configure, or activation
  // failed, then the associated message is shown; otherwise use the last_error
  // value for the network or a Shill property if available.
  void ShowNetworkConnectErrorForGuid(const std::string& error_name,
                                      const std::string& guid);

  // Shows a notification indicating the device is unlocked by the carrier and
  // can now connect to any available cellular network.
  void ShowCarrierUnlockNotification();

  // Show a mobile activation error notification.
  void ShowMobileActivationErrorForGuid(const std::string& guid);

  void set_system_tray_client(SystemTrayClient* system_tray_client) {
    system_tray_client_ = system_tray_client;
  }

  static const char kNetworkConnectNotificationId[];
  static const char kNetworkActivateNotificationId[];
  static const char kNetworkOutOfCreditsNotificationId[];
  static const char kNetworkCarrierUnlockNotificationId[];

 private:
  friend class NetworkStateNotifierTest;

  struct VpnDetails {
    VpnDetails(const std::string& guid, const std::string& name)
        : guid(guid), name(name) {}
    std::string guid;
    std::string name;
  };

  // NetworkConnectionObserver
  void ConnectToNetworkRequested(const std::string& service_path) override;
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;
  void DisconnectRequested(const std::string& service_path) override;

  // NetworkStateHandlerObserver
  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void NetworkIdentifierTransitioned(const std::string& old_service_path,
                                     const std::string& new_service_path,
                                     const std::string& old_guid,
                                     const std::string& new_guid) override;
  void OnShuttingDown() override;

  void OnConnectErrorGetProperties(
      const std::string& error_name,
      const std::string& service_path,
      std::optional<base::Value::Dict> shill_properties);

  void ShowConnectErrorNotification(
      const std::string& error_name,
      const std::string& service_path,
      std::optional<base::Value::Dict> shill_properties);

  void ShowVpnDisconnectedNotification(VpnDetails* vpn);

  // Removes any existing connect notifications.
  void RemoveConnectNotification();

  // Removes any existing carrier unlock notifications.
  void RemoveCarrierUnlockNotification();

  // Returns true if the default network changed.
  bool UpdateDefaultNetwork(const NetworkState* network);

  // Helper methods to update state and check for notifications.
  void UpdateVpnConnectionState(const NetworkState* active_vpn);
  void UpdateCellularOutOfCredits();
  void UpdateCellularActivating(const NetworkState* cellular);

  // Shows the network settings for |network_id|.
  void ShowNetworkSettings(const std::string& network_id);
  void ShowSimUnlockSettings();
  void ShowMobileDataSubpage();
  void ShowApnSettings(const std::string& network_id);

  // Shows the carrier account detail page for |network_id|.
  void ShowCarrierAccountDetail(const std::string& network_id);

  raw_ptr<SystemTrayClient, DanglingUntriaged> system_tray_client_ = nullptr;

  // The details of the connected VPN network if any, otherwise null.
  // Used for displaying the VPN disconnected notification.
  std::unique_ptr<VpnDetails> connected_vpn_;

  // Tracks state for out of credits notification.
  bool did_show_out_of_credits_ = false;
  base::Time out_of_credits_notify_time_;
  // Set to the GUID of the active non VPN network if any, otherwise empty.
  std::string active_non_vpn_network_guid_;

  // Set to the GUID of the current network which spawned a connection error
  // notification if any, otherwise empty.
  std::string connect_error_notification_network_guid_;

  // Tracks GUIDs of activating cellular networks for activation notification.
  std::set<std::string> cellular_activating_guids_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<NetworkStateNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_STATE_NOTIFIER_H_
