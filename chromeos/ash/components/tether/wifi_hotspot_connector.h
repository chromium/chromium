// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_CONNECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace ash {

class NetworkConnect;
class NetworkState;

namespace tether {

// Connects to a Wi-Fi hotspot, given an SSID and password.
class WifiHotspotConnector : public NetworkStateHandlerObserver {
 public:
  enum class WifiHotspotConnectionError {
    kTimeout,
    kWifiHotspotConnectorClassDestroyed,
    kCancelledForNewerConnectionAttempt,
    kNetworkConnectionHandlerFailed,
    kNetworkStateWasNull,
    kWifiFailedToEnabled,
  };

  WifiHotspotConnector(NetworkHandler* network_handler,
                       NetworkConnect* network_connect);

  WifiHotspotConnector(const WifiHotspotConnector&) = delete;
  WifiHotspotConnector& operator=(const WifiHotspotConnector&) = delete;

  ~WifiHotspotConnector() override;

  // Function which receives the GUID of the connected Wi-Fi hotspot. If
  // the string passed is empty, an error occurred trying to connect.
  using WifiConnectionCallback = base::OnceCallback<void(
      base::expected<std::string, WifiHotspotConnectionError>)>;

  // Connects to the Wi-Fi network with SSID |ssid| and password |password|,
  // invoking |callback| when the connection succeeds, fails, or times out.
  // Note: If ConnectToWifiHotspot() is called while another connection attempt
  // is in progress, the previous attempt will be canceled and the new attempt
  // will begin.
  virtual void ConnectToWifiHotspot(const std::string& ssid,
                                    const std::string& password,
                                    const std::string& tether_network_guid,
                                    WifiConnectionCallback callback);

  void OnEnableWifiError(const std::string& error_name);

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void OnShuttingDown() override;

 private:
  friend class WifiHotspotConnectorTest;

  static const int64_t kConnectionTimeoutSeconds = 20;
  static const int64_t kMaxWifiConnectionAttempts = 3;

  void UpdateWaitingForWifi();
  void InitiateConnectionToCurrentNetwork();
  void CompleteActiveConnectionAttempt(
      std::optional<WifiHotspotConnectionError> error);
  void CreateWifiConfiguration();
  void RequestWifiScan();
  base::Value::Dict CreateWifiPropertyDictionary(const std::string& ssid,
                                                 const std::string& password);
  void OnConnectionTimeout();

  void OnWifiConnectionSucceeded();
  void OnWifiConnectionFailed(const std::string& error_name);

  void AssociateNetworks(std::string wifi_network_guid,
                         std::string tether_network_guid);

  void SetTestDoubles(std::unique_ptr<base::OneShotTimer> test_timer,
                      base::Clock* test_clock,
                      scoped_refptr<base::TaskRunner> test_task_runner);

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  raw_ptr<NetworkConnect, DanglingUntriaged> network_connect_;
  raw_ptr<NetworkHandler> network_handler_;
  std::unique_ptr<base::OneShotTimer> timer_;
  raw_ptr<base::Clock> clock_;

  std::string ssid_;
  std::string password_;
  std::string tether_network_guid_;
  std::string wifi_network_guid_;
  WifiConnectionCallback callback_;
  int current_connection_attempt_count_ = 1;
  bool has_requested_wifi_scan_ = false;
  bool is_waiting_for_wifi_to_enable_ = false;
  bool has_initiated_connection_to_current_network_ = false;
  base::Time connection_attempt_start_time_;
  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<WifiHotspotConnector> weak_ptr_factory_{this};
};

std::ostream& operator<<(
    std::ostream& stream,
    const WifiHotspotConnector::WifiHotspotConnectionError error);

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_CONNECTOR_H_
