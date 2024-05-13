// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

ConnectionInfoMetricsLogger::ConnectionInfo::ConnectionInfo(
    const NetworkState* network,
    bool is_user_initiated)
    : guid(network->guid()),
      shill_error(network->GetError()),
      is_user_initiated(is_user_initiated) {
  if (network->IsConnectedState()) {
    status = Status::kConnected;
  } else if (network->IsConnectingState()) {
    status = Status::kConnecting;
  } else if (network->connection_state() == shill::kStateDisconnecting) {
    status = Status::kDisconnecting;
  } else if (network->connection_state() == shill::kStateFailure) {
    status = Status::kFailure;
  } else {
    status = Status::kDisconnected;
  }
}

ConnectionInfoMetricsLogger::ConnectionInfo::~ConnectionInfo() = default;

bool ConnectionInfoMetricsLogger::ConnectionInfo::operator==(
    const ConnectionInfoMetricsLogger::ConnectionInfo& other) const {
  return status == other.status && guid == other.guid &&
         is_user_initiated == other.is_user_initiated &&
         shill_error == other.shill_error;
}

ConnectionInfoMetricsLogger::ConnectionInfoMetricsLogger() = default;

ConnectionInfoMetricsLogger::~ConnectionInfoMetricsLogger() {
  if (network_connection_handler_)
    network_connection_handler_->RemoveObserver(this);
}

void ConnectionInfoMetricsLogger::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler) {
  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }

  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_observer_.Observe(network_state_handler_.get());
    NetworkListChanged();
  }
}

void ConnectionInfoMetricsLogger::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ConnectionInfoMetricsLogger::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ConnectionInfoMetricsLogger::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkList(&network_list);

  base::flat_set<std::string> visible_guids;
  for (const auto* network : network_list) {
    UpdateConnectionInfo(network);
    visible_guids.insert(network->guid());
  }

  base::flat_map<std::string, ConnectionInfo> old_guid_to_connection_info;
  old_guid_to_connection_info.swap(guid_to_connection_info_);

  // Only store visible network ConnectionInfo in |guid_to_connection_info_|.
  for (const auto& connection_info : old_guid_to_connection_info) {
    const std::string& guid = connection_info.first;
    if (!base::Contains(visible_guids, guid))
      continue;
    guid_to_connection_info_.insert_or_assign(
        guid, old_guid_to_connection_info.find(guid)->second);
  }
}

void ConnectionInfoMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  UpdateConnectionInfo(network);
}

void ConnectionInfoMetricsLogger::ConnectToNetworkRequested(
    const std::string& service_path) {
  if (!network_state_handler_) {
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network) {
    return;
  }

  guid_to_connection_info_.insert_or_assign(
      network->guid(), ConnectionInfo(network,
                                      /*is_user_initiated=*/true));
}

void ConnectionInfoMetricsLogger::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void ConnectionInfoMetricsLogger::ConnectSucceeded(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network)
    return;

  // Update the connection request to no longer be "user initiated" so that we
  // don't continue to emit subsequent connections as user initiated.
  guid_to_connection_info_.insert_or_assign(
      network->guid(), ConnectionInfo(network, /*is_user_initiated=*/false));
  NetworkMetricsHelper::LogUserInitiatedConnectionResult(network->guid());
}

void ConnectionInfoMetricsLogger::ConnectFailed(const std::string& service_path,
                                                const std::string& error_name) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network)
    return;

  // Update the connection request to no longer be "user initiated" so that we
  // don't continue to emit subsequent connections as user initiated.
  guid_to_connection_info_.insert_or_assign(
      network->guid(), ConnectionInfo(network, /*is_user_initiated=*/false));
  NetworkMetricsHelper::LogUserInitiatedConnectionResult(network->guid(),
                                                         error_name);
}

void ConnectionInfoMetricsLogger::UpdateConnectionInfo(
    const NetworkState* network) {
  const std::optional<ConnectionInfo> prev_info =
      GetCachedInfo(network->guid());
  // If a connect has been requested, maintain the connect request until the
  // connect succeeded or failed.
  const ConnectionInfo curr_info =
      ConnectionInfo(network, prev_info && prev_info->is_user_initiated);

  // No updates if the ConnectionInfo did not change.
  if (prev_info == curr_info)
    return;

  // If the connection status has changed, attempt to log automatic connection
  // and disconnection metrics.
  if (!prev_info || prev_info->status != curr_info.status) {
    ConnectionAttemptFinished(prev_info, curr_info);
    AttemptLogConnectionStateResult(prev_info, curr_info);
  }
  guid_to_connection_info_.insert_or_assign(network->guid(), curr_info);
}

void ConnectionInfoMetricsLogger::ConnectionAttemptFinished(
    const std::optional<ConnectionInfo>& prev_info,
    const ConnectionInfo& curr_info) const {
  DCHECK(!prev_info || prev_info && prev_info->guid == curr_info.guid);

  if (curr_info.status == ConnectionInfo::Status::kConnected) {
    NetworkMetricsHelper::LogAllConnectionResult(curr_info.guid,
                                                 !curr_info.is_user_initiated,
                                                 /*is_repeated_error=*/false);
    NotifyConnectionResult(curr_info.guid, /*shill_error=*/std::nullopt);
  }

  // If the network goes from connecting or disconnecting state to the
  // disconnected state, log the shill error if it's valid.
  if (prev_info &&
      (prev_info->status == ConnectionInfo::Status::kConnecting ||
       prev_info->status == ConnectionInfo::Status::kDisconnecting) &&
      curr_info.status == ConnectionInfo::Status::kDisconnected &&
      NetworkState::ErrorIsValid(curr_info.shill_error)) {
    NetworkMetricsHelper::LogAllConnectionResult(
        curr_info.guid, !curr_info.is_user_initiated,
        /*is_repeated_error=*/prev_info->shill_error == curr_info.shill_error,
        curr_info.shill_error);
    NotifyConnectionResult(curr_info.guid, curr_info.shill_error);
  }
}

void ConnectionInfoMetricsLogger::AttemptLogConnectionStateResult(
    const std::optional<ConnectionInfo>& prev_info,
    const ConnectionInfo& curr_info) const {
  if (curr_info.status == ConnectionInfo::Status::kConnected) {
    NetworkMetricsHelper::LogConnectionStateResult(
        curr_info.guid, NetworkMetricsHelper::ConnectionState::kConnected,
        /*shill_error=*/std::nullopt);
    return;
  }

  // If the network becomes disconnected or disconnecting from a connected state
  // as a result of a shill error.
  if (prev_info && prev_info->status == ConnectionInfo::Status::kConnected &&
      (curr_info.status == ConnectionInfo::Status::kDisconnected ||
       curr_info.status == ConnectionInfo::Status::kFailure) &&
      NetworkState::ErrorIsValid(curr_info.shill_error)) {
    NetworkMetricsHelper::LogConnectionStateResult(
        curr_info.guid,
        NetworkMetricsHelper::ConnectionState::kDisconnectedWithoutUserAction,
        ShillErrorToConnectResult(curr_info.shill_error));
    return;
  }
}

std::optional<ConnectionInfoMetricsLogger::ConnectionInfo>
ConnectionInfoMetricsLogger::GetCachedInfo(const std::string& guid) const {
  const auto prev_info_it = guid_to_connection_info_.find(guid);
  if (prev_info_it == guid_to_connection_info_.end())
    return std::nullopt;
  return prev_info_it->second;
}

void ConnectionInfoMetricsLogger::NotifyConnectionResult(
    const std::string& guid,
    const std::optional<std::string>& shill_error) const {
  for (auto& observer : observers_)
    observer.OnConnectionResult(guid, shill_error);
}

}  // namespace ash
