// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_INFO_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_INFO_METRICS_LOGGER_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkConnectionHandler;
class NetworkState;
class NetworkStateHandler;

// Class for tracking general connection information about networks.
//
// This class adds observers on network state and makes the following
// measurements on all networks:
// 1. Success rate of all connection attempts.
// 2. Success rate of user initiated connection attempts.
// 3. Connected states and non-user initiated disconnections.
//
// Note: This class does not start logging metrics until Init() is
// invoked.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ConnectionInfoMetricsLogger
    : public NetworkStateHandlerObserver,
      public NetworkConnectionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when a connection result finishes, i.e. a network becomes
    // connected from a non-connected state or a network goes from a connecting
    // state to a disconnected state. In the latter situation, |shill_error|
    // will be non-empty.
    virtual void OnConnectionResult(
        const std::string& guid,
        const std::optional<std::string>& shill_error) = 0;
  };

  ConnectionInfoMetricsLogger();
  ConnectionInfoMetricsLogger(const ConnectionInfoMetricsLogger&) = delete;
  ConnectionInfoMetricsLogger& operator=(const ConnectionInfoMetricsLogger&) =
      delete;
  ~ConnectionInfoMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConnectionHandler* network_connection_handler);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class ConnectionInfoMetricsLoggerTest;

  // Stores connection related information for a network.
  struct ConnectionInfo {
   public:
    enum class Status {
      // The network is not connected, connecting, or disconnecting.
      kDisconnected = 0,

      // The network is being connected to.
      kConnecting = 1,

      // The network is connected.
      kConnected = 2,

      // The network is disconnecting.
      kDisconnecting = 3,

      // The network is in failure state which mapped to the corresponding
      // shill error.
      kFailure = 4,
    };

    explicit ConnectionInfo(const NetworkState* network,
                            bool is_user_initiated);
    ~ConnectionInfo();

    bool operator==(const ConnectionInfo& other) const;

    Status status;
    std::string guid;
    std::string shill_error;
    bool is_user_initiated;
  };

  // NetworkStateHandlerObserver::
  void ConnectToNetworkRequested(const std::string& service_path) override;
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // NetworkConnectionObserver::
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;

  void UpdateConnectionInfo(const NetworkState* network);
  void ConnectionAttemptFinished(const std::optional<ConnectionInfo>& prev_info,
                                 const ConnectionInfo& curr_info) const;
  void AttemptLogConnectionStateResult(
      const std::optional<ConnectionInfo>& prev_info,
      const ConnectionInfo& curr_info) const;
  std::optional<ConnectionInfo> GetCachedInfo(const std::string& guid) const;
  void NotifyConnectionResult(
      const std::string& guid,
      const std::optional<std::string>& shill_error) const;

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<NetworkConnectionHandler> network_connection_handler_ = nullptr;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  // Stores connection information for all networks.
  base::flat_map<std::string, ConnectionInfo> guid_to_connection_info_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_INFO_METRICS_LOGGER_H_
