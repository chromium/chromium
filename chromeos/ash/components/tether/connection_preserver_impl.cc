// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/connection_preserver_impl.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

namespace ash::tether {

ConnectionPreserverImpl::ConnectionPreserverImpl(
    HostConnection::Factory* host_connection_factory,
    NetworkStateHandler* network_state_handler,
    ActiveHost* active_host,
    TetherHostResponseRecorder* tether_host_response_recorder)
    : host_connection_factory_(host_connection_factory),
      network_state_handler_(network_state_handler),
      active_host_(active_host),
      tether_host_response_recorder_(tether_host_response_recorder),
      preserved_connection_timer_(std::make_unique<base::OneShotTimer>()) {
  active_host_->AddObserver(this);
}

ConnectionPreserverImpl::~ConnectionPreserverImpl() {
  active_host_->RemoveObserver(this);
  RemovePreservedConnectionIfPresent();
}

void ConnectionPreserverImpl::HandleSuccessfulTetherAvailabilityResponse(
    const std::string& device_id) {
  DCHECK(!device_id.empty());

  // Do not bother preserving a BLE Connection if the device is already
  // connected to the Internet. BLE and Wi-Fi share the same antenna, so keeping
  // a BLE connection open while there's an active Wi-Fi connection would
  // degrade the Wi-Fi network's performance.
  if (IsConnectedToInternet()) {
    return;
  }

  // No BLE Connection has been preserved yet, so simply preserve this first
  // request.
  if (preserved_connection_device_id_.empty()) {
    SetPreservedConnection(device_id);
    return;
  }

  // If the requested |device_id| is a higher priority than the current
  // preserved Connection, make it the new preserved Connection. Only a single
  // BLE Connection is preserved at once (there are negative performance
  // implications with keeping multiple open).
  if (preserved_connection_device_id_ !=
      GetPreferredPreservedConnectionDeviceId(device_id)) {
    RemovePreservedConnectionIfPresent();
    SetPreservedConnection(device_id);
  } else {
    PA_LOG(VERBOSE)
        << "The connection to device with ID "
        << TetherHost::TruncateDeviceIdForLogs(device_id)
        << " was not preserved; another device has higher priority.";
  }
}

void ConnectionPreserverImpl::OnDisconnected() {
  PA_LOG(VERBOSE) << "Remote device disconnected from this device: "
                  << TetherHost::TruncateDeviceIdForLogs(
                         preserved_connection_device_id_);
  RemovePreservedConnectionIfPresent();
}

void ConnectionPreserverImpl::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper) {
  // Do nothing.
}

void ConnectionPreserverImpl::OnConnectionAttemptFinished(
    std::unique_ptr<HostConnection> host_connection) {
  if (!host_connection) {
    PA_LOG(WARNING) << "Failed to connect to device "
                    << TetherHost::TruncateDeviceIdForLogs(
                           preserved_connection_device_id_);
    RemovePreservedConnectionIfPresent();
  } else {
    preserved_host_connection_ = std::move(host_connection);
    PA_LOG(VERBOSE) << "Successfully preserved connection for device: "
                    << TetherHost::TruncateDeviceIdForLogs(
                           preserved_connection_device_id_);
  }
}

void ConnectionPreserverImpl::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& change_info) {
  if (change_info.new_status == ActiveHost::ActiveHostStatus::CONNECTED) {
    RemovePreservedConnectionIfPresent();
  }
}

bool ConnectionPreserverImpl::IsConnectedToInternet() {
  // If a network is active (i.e., connecting or connected), it will be returned
  // at the front of the list, so using FirstNetworkByType() guarantees that we
  // will find an active network if there is one.
  const NetworkState* first_network =
      network_state_handler_->FirstNetworkByType(NetworkTypePattern::Default());
  return first_network && first_network->IsConnectingOrConnected();
}

std::string ConnectionPreserverImpl::GetPreferredPreservedConnectionDeviceId(
    const std::string& device_id) {
  DCHECK(!preserved_connection_device_id_.empty());

  // Between |device_id| and |preserved_connection_device_id_|, prefer whichever
  // appears in |previously_connected_host_ids| first, since
  // |previously_connected_host_ids| is ordered with most recently connected
  // hosts first. |device_id| is preferred over
  // |preserved_connection_device_id_| as a tie-breaker.

  std::vector<std::string> previously_connected_host_ids =
      tether_host_response_recorder_->GetPreviouslyConnectedHostIds();
  const auto preferred_connection_id_it = base::ranges::find_if(
      previously_connected_host_ids,
      [this, &device_id](auto previously_connected_id) {
        // Pick out whichever ID appears in the list first.
        return previously_connected_id == device_id ||
               previously_connected_id == preserved_connection_device_id_;
      });

  std::string preferred_connection_id =
      preferred_connection_id_it != previously_connected_host_ids.end()
          ? *preferred_connection_id_it
          : device_id;
  return preferred_connection_id;
}

void ConnectionPreserverImpl::SetPreservedConnection(
    const std::string& device_id) {
  DCHECK(preserved_connection_device_id_.empty());

  PA_LOG(VERBOSE) << "Preserving connection to device with ID "
                  << TetherHost::TruncateDeviceIdForLogs(device_id) << ".";

  preserved_connection_device_id_ = device_id;

  host_connection_factory_->ScanForTetherHostAndCreateConnection(
      device_id, HostConnection::Factory::ConnectionPriority::kLow,
      /*payload_listener=*/this,
      base::BindOnce(&ConnectionPreserverImpl::OnDisconnected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ConnectionPreserverImpl::OnConnectionAttemptFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  preserved_connection_timer_->Start(
      FROM_HERE, base::Seconds(kTimeoutSeconds),
      base::BindOnce(
          &ConnectionPreserverImpl::RemovePreservedConnectionIfPresent,
          weak_ptr_factory_.GetWeakPtr()));
}

void ConnectionPreserverImpl::RemovePreservedConnectionIfPresent() {
  if (preserved_connection_device_id_.empty()) {
    return;
  }

  PA_LOG(VERBOSE) << "Removing preserved connection to device with ID "
                  << TetherHost::TruncateDeviceIdForLogs(
                         preserved_connection_device_id_)
                  << ".";

  preserved_host_connection_.reset();

  preserved_connection_device_id_.clear();
  preserved_connection_timer_->Stop();
}

void ConnectionPreserverImpl::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer_for_test) {
  preserved_connection_timer_ = std::move(timer_for_test);
}

}  // namespace ash::tether
