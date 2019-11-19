// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_network_disconnection_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/tether_session_completion_logger.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

namespace tether {

TetherNetworkDisconnectionHandler::TetherNetworkDisconnectionHandler(
    ActiveHost* active_host,
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationRemover* network_configuration_remover,
    DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
    TetherSessionCompletionLogger* tether_session_completion_logger)
    : active_host_(active_host),
      network_state_handler_(network_state_handler),
      network_configuration_remover_(network_configuration_remover),
      disconnect_tethering_request_sender_(disconnect_tethering_request_sender),
      tether_session_completion_logger_(tether_session_completion_logger),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  network_state_handler_->AddObserver(this, FROM_HERE);
}

TetherNetworkDisconnectionHandler::~TetherNetworkDisconnectionHandler() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void TetherNetworkDisconnectionHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  // Only handle network connection state changes which indicate that the
  // underlying Wi-Fi network for a Tether connection has been disconnected.
  if (network->guid() != active_host_->GetWifiNetworkGuid() ||
      network->IsConnectingOrConnected()) {
    return;
  }

  // Handle disconnection as part of a new task. Posting a task here ensures
  // that processing the disconnection is done after other
  // NetworkStateHandlerObservers are finished running. Processing the
  // disconnection immediately can cause crashes; see https://crbug.com/800370.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TetherNetworkDisconnectionHandler::
                                    HandleActiveWifiNetworkDisconnection,
                                weak_ptr_factory_.GetWeakPtr(), network->guid(),
                                network->path()));
}

void TetherNetworkDisconnectionHandler::HandleActiveWifiNetworkDisconnection(
    const std::string& network_guid,
    const std::string& network_path) {
  PA_LOG(WARNING) << "Connection to active host (Wi-Fi network GUID "
                  << network_guid << ") has been lost.";

  // Check if Wi-Fi is enabled; if it is, this indicates that the connection
  // to the Tether host dropped. If it isn't, then the event of Wi-Fi being
  // disabled caused the connection to end.
  tether_session_completion_logger_->RecordTetherSessionCompletion(
      network_state_handler_->IsTechnologyEnabled(NetworkTypePattern::WiFi())
          ? TetherSessionCompletionLogger::SessionCompletionReason::
                CONNECTION_DROPPED
          : TetherSessionCompletionLogger::SessionCompletionReason::
                WIFI_DISABLED);

  network_configuration_remover_->RemoveNetworkConfigurationByPath(
      network_path);

  // Send a DisconnectTetheringRequest to the tether host so that it can shut
  // down its Wi-Fi hotspot if it is no longer in use.
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      active_host_->GetActiveHostDeviceId());

  active_host_->SetActiveHostDisconnected();
}

void TetherNetworkDisconnectionHandler::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> test_task_runner) {
  task_runner_ = test_task_runner;
}

}  // namespace tether

}  // namespace chromeos
