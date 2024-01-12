// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/tether/active_host.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace ash {

class NetworkStateHandler;

namespace tether {

class ActiveHost;
class DisconnectTetheringRequestSender;
class NetworkConfigurationRemover;
class TetherSessionCompletionLogger;

// Handles lost Wi-Fi connections for ongoing tether sessions. When a tether
// connection is in progress, the device is connected to an underlying Wi-Fi
// network. If the tether network is disconnected, tether metadata is
// updated accordingly, and a message is sent to the host to disable its
// Wi-Fi hotspot.
class TetherNetworkDisconnectionHandler : public NetworkStateHandlerObserver {
 public:
  TetherNetworkDisconnectionHandler(
      ActiveHost* active_host,
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationRemover* network_configuration_remover,
      DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
      TetherSessionCompletionLogger* tether_session_completion_logger);

  TetherNetworkDisconnectionHandler(const TetherNetworkDisconnectionHandler&) =
      delete;
  TetherNetworkDisconnectionHandler& operator=(
      const TetherNetworkDisconnectionHandler&) = delete;

  ~TetherNetworkDisconnectionHandler() override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

 private:
  friend class TetherNetworkDisconnectionHandlerTest;

  void HandleActiveWifiNetworkDisconnection(const std::string& network_guid,
                                            const std::string& network_path);

  void SetTaskRunnerForTesting(
      scoped_refptr<base::TaskRunner> test_task_runner);

  raw_ptr<ActiveHost> active_host_;
  raw_ptr<NetworkStateHandler> network_state_handler_;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  raw_ptr<NetworkConfigurationRemover> network_configuration_remover_;
  raw_ptr<DisconnectTetheringRequestSender>
      disconnect_tethering_request_sender_;
  raw_ptr<TetherSessionCompletionLogger> tether_session_completion_logger_;

  scoped_refptr<base::TaskRunner> task_runner_;
  base::WeakPtrFactory<TetherNetworkDisconnectionHandler> weak_ptr_factory_{
      this};
};

}  // namespace tether
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_
