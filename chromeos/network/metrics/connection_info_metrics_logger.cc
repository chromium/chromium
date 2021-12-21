// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/connection_info_metrics_logger.h"

#include "base/containers/flat_set.h"
#include "chromeos/network/metrics/network_metrics_helper.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

ConnectionInfoMetricsLogger::ConnectionInfo::ConnectionInfo(
    const NetworkState* network,
    bool was_disconnect_requested)
    : guid(network->guid()),
      shill_error(network->GetError()),
      was_disconnect_requested(was_disconnect_requested) {
  if (network->IsConnectedState())
    status = Status::kConnected;
  else if (network->IsConnectingState())
    status = Status::kConnecting;
  else
    status = Status::kDisconnected;
}

ConnectionInfoMetricsLogger::ConnectionInfo::~ConnectionInfo() = default;

bool ConnectionInfoMetricsLogger::ConnectionInfo::operator==(
    const ConnectionInfoMetricsLogger::ConnectionInfo& other) const {
  return status == other.status &&
         was_disconnect_requested == other.was_disconnect_requested &&
         guid == other.guid && shill_error == other.shill_error;
}

ConnectionInfoMetricsLogger::ConnectionInfoMetricsLogger() = default;

ConnectionInfoMetricsLogger::~ConnectionInfoMetricsLogger() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
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
    network_state_handler_->AddObserver(this, FROM_HERE);
    NetworkListChanged();
  }
}

void ConnectionInfoMetricsLogger::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkList(&network_list);

  base::flat_set<std::string> visible_guids;
  for (const auto* network : network_list) {
    UpdateConnectionInfo(network);
    visible_guids.insert(network->guid());
  }

  // Remove networks that are no longer visible.
  for (const auto& connection_info : guid_to_connection_info_) {
    const std::string guid = connection_info.first;
    if (visible_guids.find(guid) == visible_guids.end())
      guid_to_connection_info_.erase(guid);
  }
}

void ConnectionInfoMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  UpdateConnectionInfo(network);
}

void ConnectionInfoMetricsLogger::DisconnectRequested(
    const std::string& service_path) {
  if (!network_state_handler_)
    return;

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network)
    return;

  UpdateConnectionInfo(network, /*disconnect_requested=*/true);
}

void ConnectionInfoMetricsLogger::UpdateConnectionInfo(
    const NetworkState* network,
    bool disconnect_requested) {
  const absl::optional<ConnectionInfo> prev_info =
      GetCachedInfo(network->guid());
  ConnectionInfo curr_info =
      ConnectionInfo(network,
                     /*was_disconnect_requested=*/disconnect_requested);

  // No updates if the ConnectionInfo did not change.
  if (prev_info == curr_info)
    return;

  // If a disconnect has been requested, maintain it until the status changes.
  if (prev_info && prev_info->status == curr_info.status)
    curr_info.was_disconnect_requested |= prev_info->was_disconnect_requested;
  else
    AttemptLogAllConnectionResult(prev_info, curr_info);

  guid_to_connection_info_.insert_or_assign(network->guid(), curr_info);
}

void ConnectionInfoMetricsLogger::AttemptLogAllConnectionResult(
    const absl::optional<ConnectionInfo>& prev_info,
    const ConnectionInfo& curr_info) const {
  DCHECK(!prev_info || prev_info && prev_info->guid == curr_info.guid);

  if (curr_info.status == ConnectionInfo::Status::kConnected)
    NetworkMetricsHelper::LogAllConnectionResult(curr_info.guid);

  if (prev_info && !prev_info->was_disconnect_requested &&
      prev_info->status == ConnectionInfo::Status::kConnecting &&
      curr_info.status == ConnectionInfo::Status::kDisconnected) {
    NetworkMetricsHelper::LogAllConnectionResult(curr_info.guid,
                                                 curr_info.shill_error);
  }
}

absl::optional<ConnectionInfoMetricsLogger::ConnectionInfo>
ConnectionInfoMetricsLogger::GetCachedInfo(const std::string& guid) const {
  const auto prev_info_it = guid_to_connection_info_.find(guid);
  if (prev_info_it == guid_to_connection_info_.end())
    return absl::nullopt;
  return prev_info_it->second;
}

}  // namespace chromeos