// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_request_sender_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace ash {

namespace tether {

// static
DisconnectTetheringRequestSenderImpl::Factory*
    DisconnectTetheringRequestSenderImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<DisconnectTetheringRequestSender>
DisconnectTetheringRequestSenderImpl::Factory::Create(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostFetcher* tether_host_fetcher) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        device_sync_client, secure_channel_client, tether_host_fetcher);
  }

  return base::WrapUnique(new DisconnectTetheringRequestSenderImpl(
      device_sync_client, secure_channel_client, tether_host_fetcher));
}

// static
void DisconnectTetheringRequestSenderImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

DisconnectTetheringRequestSenderImpl::Factory::~Factory() = default;

DisconnectTetheringRequestSenderImpl::DisconnectTetheringRequestSenderImpl(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    TetherHostFetcher* tether_host_fetcher)
    : device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      tether_host_fetcher_(tether_host_fetcher) {}

DisconnectTetheringRequestSenderImpl::~DisconnectTetheringRequestSenderImpl() {
  for (auto const& entry : device_id_to_operation_map_)
    entry.second->RemoveObserver(this);
}

void DisconnectTetheringRequestSenderImpl::SendDisconnectRequestToDevice(
    const std::string& device_id) {
  if (base::Contains(device_id_to_operation_map_, device_id))
    return;

  num_pending_host_fetches_++;
  tether_host_fetcher_->FetchTetherHost(
      device_id,
      base::BindOnce(&DisconnectTetheringRequestSenderImpl::OnTetherHostFetched,
                     weak_ptr_factory_.GetWeakPtr(), device_id));
}

bool DisconnectTetheringRequestSenderImpl::HasPendingRequests() {
  return !device_id_to_operation_map_.empty() || num_pending_host_fetches_ > 0;
}

void DisconnectTetheringRequestSenderImpl::OnTetherHostFetched(
    const std::string& device_id,
    absl::optional<multidevice::RemoteDeviceRef> tether_host) {
  num_pending_host_fetches_--;
  DCHECK(num_pending_host_fetches_ >= 0);

  if (!tether_host) {
    PA_LOG(ERROR) << "Could not fetch device with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id)
                  << ". Unable to send DisconnectTetheringRequest.";
    return;
  }

  PA_LOG(VERBOSE) << "Attempting to send DisconnectTetheringRequest to device "
                  << "with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id);

  std::unique_ptr<DisconnectTetheringOperation> disconnect_tethering_operation =
      DisconnectTetheringOperation::Factory::Create(
          *tether_host, device_sync_client_, secure_channel_client_);

  // Add to the map.
  device_id_to_operation_map_.emplace(
      device_id, std::move(disconnect_tethering_operation));

  // Start the operation; OnOperationFinished() will be called when finished.
  device_id_to_operation_map_.at(device_id)->AddObserver(this);
  device_id_to_operation_map_.at(device_id)->Initialize();
}

void DisconnectTetheringRequestSenderImpl::OnOperationFinished(
    const std::string& device_id,
    bool success) {
  if (success) {
    PA_LOG(VERBOSE) << "Successfully sent DisconnectTetheringRequest to device "
                    << "with ID "
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id);
  } else {
    PA_LOG(ERROR) << "Failed to send DisconnectTetheringRequest to device "
                  << "with ID "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id);
  }

  bool had_pending_requests = HasPendingRequests();

  if (base::Contains(device_id_to_operation_map_, device_id)) {
    // Regardless of success/failure, unregister as a listener and delete the
    // operation.
    device_id_to_operation_map_.at(device_id)->RemoveObserver(this);
    device_id_to_operation_map_.erase(device_id);
  } else {
    PA_LOG(ERROR)
        << "Operation finished, but device with ID "
        << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(device_id)
        << " was not being tracked by DisconnectTetheringRequestSender.";
  }

  // If there were pending reqests but now there are none, notify the Observers.
  if (had_pending_requests && !HasPendingRequests())
    NotifyPendingDisconnectRequestsComplete();
}

}  // namespace tether

}  // namespace ash
