// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/tether_disconnector.h"

namespace chromeos {

namespace tether {

namespace {

void OnFailedDisconnectionFromPreviousHost(
    const std::string& tether_network_guid,
    const std::string& error_name) {
  PA_LOG(ERROR) << "Failed to disconnect from previously-active host. "
                << "GUID: " << tether_network_guid << ", Error: " << error_name;
}

}  // namespace

NetworkConnectionHandlerTetherDelegate::Callbacks::Callbacks(
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback)
    : success_callback(success_callback), error_callback(error_callback) {}

NetworkConnectionHandlerTetherDelegate::Callbacks::Callbacks(
    const Callbacks& other) = default;

NetworkConnectionHandlerTetherDelegate::Callbacks::~Callbacks() = default;

NetworkConnectionHandlerTetherDelegate::NetworkConnectionHandlerTetherDelegate(
    NetworkConnectionHandler* network_connection_handler,
    ActiveHost* active_host,
    TetherConnector* tether_connector,
    TetherDisconnector* tether_disconnector)
    : network_connection_handler_(network_connection_handler),
      active_host_(active_host),
      tether_connector_(tether_connector),
      tether_disconnector_(tether_disconnector) {
  network_connection_handler_->SetTetherDelegate(this);
}

NetworkConnectionHandlerTetherDelegate::
    ~NetworkConnectionHandlerTetherDelegate() {
  // If there are still pending callbacks, invoke them here. It should never be
  // possible for the Tether component to shut down with pending callbacks.
  for (const auto& entry : request_num_to_callbacks_map_) {
    entry.second.error_callback.Run(
        NetworkConnectionHandler::kErrorConnectFailed);
  }

  network_connection_handler_->SetTetherDelegate(nullptr);
}

void NetworkConnectionHandlerTetherDelegate::DisconnectFromNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  int request_num = next_request_num_++;
  request_num_to_callbacks_map_.emplace(
      request_num, Callbacks(success_callback, error_callback));
  tether_disconnector_->DisconnectFromNetwork(
      tether_network_guid,
      base::Bind(&NetworkConnectionHandlerTetherDelegate::OnRequestSuccess,
                 weak_ptr_factory_.GetWeakPtr(), request_num),
      base::Bind(&NetworkConnectionHandlerTetherDelegate::OnRequestError,
                 weak_ptr_factory_.GetWeakPtr(), request_num),
      TetherSessionCompletionLogger::SessionCompletionReason::
          USER_DISCONNECTED);
}

void NetworkConnectionHandlerTetherDelegate::ConnectToNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  if (active_host_->GetActiveHostStatus() ==
      ActiveHost::ActiveHostStatus::CONNECTED) {
    if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
      PA_LOG(WARNING) << "Received a request to connect to Tether network with "
                      << "GUID " << tether_network_guid << ", but that network "
                      << "is already connected. Ignoring this request.";
      error_callback.Run(NetworkConnectionHandler::kErrorConnected);
      return;
    }

    std::string previous_host_guid = active_host_->GetTetherNetworkGuid();
    DCHECK(!previous_host_guid.empty());

    PA_LOG(VERBOSE) << "Connection requested to GUID " << tether_network_guid
                    << ", but there is already an active connection. "
                    << "Disconnecting from network with GUID "
                    << previous_host_guid << ".";
    DisconnectFromNetwork(
        previous_host_guid, base::DoNothing(),
        base::Bind(&OnFailedDisconnectionFromPreviousHost, previous_host_guid));
  }

  int request_num = next_request_num_++;
  request_num_to_callbacks_map_.emplace(
      request_num, Callbacks(success_callback, error_callback));
  tether_connector_->ConnectToNetwork(
      tether_network_guid,
      base::Bind(&NetworkConnectionHandlerTetherDelegate::OnRequestSuccess,
                 weak_ptr_factory_.GetWeakPtr(), request_num),
      base::Bind(&NetworkConnectionHandlerTetherDelegate::OnRequestError,
                 weak_ptr_factory_.GetWeakPtr(), request_num));
}

void NetworkConnectionHandlerTetherDelegate::OnRequestSuccess(int request_num) {
  DCHECK(base::Contains(request_num_to_callbacks_map_, request_num));
  request_num_to_callbacks_map_.at(request_num).success_callback.Run();
  request_num_to_callbacks_map_.erase(request_num);
}

void NetworkConnectionHandlerTetherDelegate::OnRequestError(
    int request_num,
    const std::string& error_name) {
  DCHECK(base::Contains(request_num_to_callbacks_map_, request_num));
  request_num_to_callbacks_map_.at(request_num).error_callback.Run(error_name);
  request_num_to_callbacks_map_.erase(request_num);
}

}  // namespace tether

}  // namespace chromeos
