// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/connection_manager_impl.h"

#include "base/bind_helpers.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace chromeos {
namespace phonehub {
namespace {
const char kPhoneHubFeatureName[] = "phone_hub";
}  // namespace

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::secure_channel::SecureChannelClient* secure_channel_client)
    : multidevice_setup_client_(multidevice_setup_client),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client) {
  DCHECK(multidevice_setup_client_);
  DCHECK(device_sync_client_);
  DCHECK(secure_channel_client_);
}

ConnectionManagerImpl::~ConnectionManagerImpl() {
  if (channel_)
    channel_->RemoveObserver(this);
}

ConnectionManager::Status ConnectionManagerImpl::GetStatus() const {
  // Connection attempt was successful and with an active channel between
  // devices.
  if (channel_)
    return Status::kConnected;

  // Initiated an connection attempt and awaiting result.
  if (connection_attempt_)
    return Status::kConnecting;

  // No connection attempt has been made or if either local or host device
  // has disconnected.
  return Status::kDisconnected;
}

void ConnectionManagerImpl::AttemptConnection() {
  if (GetStatus() != Status::kDisconnected) {
    PA_LOG(WARNING) << "Connection to phone already established or is "
                    << "currently attempting to establish, exiting "
                    << "AttemptConnection().";
    return;
  }

  const base::Optional<multidevice::RemoteDeviceRef> remote_device =
      multidevice_setup_client_->GetHostStatus().second;
  const base::Optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();

  if (!remote_device || !local_device) {
    PA_LOG(ERROR) << "AttemptConnection() failed because either remote or "
                  << "local device is null.";
    return;
  }

  connection_attempt_ = secure_channel_client_->InitiateConnectionToDevice(
      *remote_device, *local_device, kPhoneHubFeatureName,
      secure_channel::ConnectionMedium::kNearbyConnections,
      secure_channel::ConnectionPriority::kMedium);
  connection_attempt_->SetDelegate(this);
  NotifyStatusChanged();
}

void ConnectionManagerImpl::SendMessage(const std::string& payload) {
  if (!channel_) {
    PA_LOG(ERROR) << "SendMessage() failed because channel is null.";
    return;
  }

  channel_->SendMessage(payload, base::DoNothing());
}

void ConnectionManagerImpl::OnConnectionAttemptFailure(
    chromeos::secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(WARNING) << "AttemptConnection() failed to establish connection with "
                  << "error: " << reason << ".";
  connection_attempt_.reset();
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnConnection(
    std::unique_ptr<chromeos::secure_channel::ClientChannel> channel) {
  PA_LOG(VERBOSE) << "AttemptConnection() successfully established a "
                  << "connection between local and remote device.";
  channel_ = std::move(channel);
  channel_->AddObserver(this);
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnDisconnected() {
  connection_attempt_.reset();
  channel_->RemoveObserver(this);
  channel_.reset();
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnMessageReceived(const std::string& payload) {
  NotifyMessageReceived(payload);
}

}  // namespace phonehub
}  // namespace chromeos
