// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_request_sender_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"

namespace ash::tether {

// static
DisconnectTetheringRequestSenderImpl::Factory*
    DisconnectTetheringRequestSenderImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<DisconnectTetheringRequestSender>
DisconnectTetheringRequestSenderImpl::Factory::Create(
    raw_ptr<HostConnection::Factory> host_connection_factory,
    TetherHostFetcher* tether_host_fetcher) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(host_connection_factory,
                                             tether_host_fetcher);
  }

  return base::WrapUnique(new DisconnectTetheringRequestSenderImpl(
      host_connection_factory, tether_host_fetcher));
}

// static
void DisconnectTetheringRequestSenderImpl::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

DisconnectTetheringRequestSenderImpl::Factory::~Factory() = default;

DisconnectTetheringRequestSenderImpl::DisconnectTetheringRequestSenderImpl(
    raw_ptr<HostConnection::Factory> host_connection_factory,
    TetherHostFetcher* tether_host_fetcher)
    : host_connection_factory_(host_connection_factory),
      tether_host_fetcher_(tether_host_fetcher) {}

DisconnectTetheringRequestSenderImpl::~DisconnectTetheringRequestSenderImpl() {
  for (auto const& entry : device_id_to_operation_map_) {
    entry.second->RemoveObserver(this);
  }
}

void DisconnectTetheringRequestSenderImpl::SendDisconnectRequestToDevice(
    const std::string& device_id) {
  if (base::Contains(device_id_to_operation_map_, device_id)) {
    return;
  }

  std::optional<multidevice::RemoteDeviceRef> tether_host =
      tether_host_fetcher_->GetTetherHost();

  if (!tether_host || tether_host->GetDeviceId() != device_id) {
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
      DisconnectTetheringOperation::Factory::Create(TetherHost(*tether_host),
                                                    host_connection_factory_);

  // Add to the map.
  device_id_to_operation_map_.emplace(
      device_id, std::move(disconnect_tethering_operation));

  // Start the operation; OnOperationFinished() will be called when finished.
  device_id_to_operation_map_.at(device_id)->AddObserver(this);
  device_id_to_operation_map_.at(device_id)->Initialize();
}

bool DisconnectTetheringRequestSenderImpl::HasPendingRequests() {
  return !device_id_to_operation_map_.empty();
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

  // If there were pending requests but now there are none, notify the
  // Observers.
  if (had_pending_requests && !HasPendingRequests()) {
    NotifyPendingDisconnectRequestsComplete();
  }
}

}  // namespace ash::tether
