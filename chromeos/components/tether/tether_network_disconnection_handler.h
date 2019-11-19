// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace chromeos {

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
  ~TetherNetworkDisconnectionHandler() override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;

 private:
  friend class TetherNetworkDisconnectionHandlerTest;

  void HandleActiveWifiNetworkDisconnection(const std::string& network_guid,
                                            const std::string& network_path);

  void SetTaskRunnerForTesting(
      scoped_refptr<base::TaskRunner> test_task_runner);

  ActiveHost* active_host_;
  NetworkStateHandler* network_state_handler_;
  NetworkConfigurationRemover* network_configuration_remover_;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender_;
  TetherSessionCompletionLogger* tether_session_completion_logger_;

  scoped_refptr<base::TaskRunner> task_runner_;
  base::WeakPtrFactory<TetherNetworkDisconnectionHandler> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(TetherNetworkDisconnectionHandler);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_
