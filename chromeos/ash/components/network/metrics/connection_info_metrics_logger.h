// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_INFO_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_INFO_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

class NetworkState;
class NetworkConnectionHandler;
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
  ConnectionInfoMetricsLogger();
  ConnectionInfoMetricsLogger(const ConnectionInfoMetricsLogger&) = delete;
  ConnectionInfoMetricsLogger& operator=(const ConnectionInfoMetricsLogger&) =
      delete;
  ~ConnectionInfoMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConnectionHandler* network_connection_handler);

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
    };

    ConnectionInfo(const NetworkState* network);
    ~ConnectionInfo();

    bool operator==(const ConnectionInfo& other) const;

    Status status;
    std::string guid;
    std::string shill_error;
  };

  // NetworkStateHandlerObserver::
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // NetworkConnectionObserver::
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;

  void UpdateConnectionInfo(const NetworkState* network);
  void AttemptLogAllConnectionResult(
      const absl::optional<ConnectionInfo>& prev_info,
      const ConnectionInfo& curr_info) const;
  void AttemptLogConnectionStateResult(
      const absl::optional<ConnectionInfo>& prev_info,
      const ConnectionInfo& curr_info) const;
  absl::optional<ConnectionInfo> GetCachedInfo(const std::string& guid) const;

  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkConnectionHandler* network_connection_handler_ = nullptr;

  // Stores connection information for all networks.
  base::flat_map<std::string, ConnectionInfo> guid_to_connection_info_;
};

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CONNECTION_INFO_METRICS_LOGGER_H_
