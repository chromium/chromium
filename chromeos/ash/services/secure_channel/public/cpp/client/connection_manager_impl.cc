// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

namespace {

constexpr base::TimeDelta kConnectionTimeout(base::Minutes(1u));

}  // namespace

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    SecureChannelClient* secure_channel_client,
    const std::string& feature_name,
    std::unique_ptr<NearbyMetricsRecorder> metrics_recorder,
    SecureChannelStructuredMetricsLogger*
        secure_channel_structured_metrics_logger)
    : ConnectionManagerImpl(multidevice_setup_client,
                            device_sync_client,
                            secure_channel_client,
                            std::make_unique<base::OneShotTimer>(),
                            feature_name,
                            std::move(metrics_recorder),
                            secure_channel_structured_metrics_logger,
                            base::DefaultClock::GetInstance()) {}

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    SecureChannelClient* secure_channel_client,
    std::unique_ptr<base::OneShotTimer> timer,
    const std::string& feature_name,
    std::unique_ptr<NearbyMetricsRecorder> metrics_recorder,
    SecureChannelStructuredMetricsLogger*
        secure_channel_structured_metrics_logger,
    base::Clock* clock)
    : multidevice_setup_client_(multidevice_setup_client),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      timer_(std::move(timer)),
      feature_name_(feature_name),
      metrics_recorder_(std::move(metrics_recorder)),
      secure_channel_structured_metrics_logger_(
          secure_channel_structured_metrics_logger),
      last_status_(Status::kDisconnected),
      status_change_timestamp_(clock->Now()),
      clock_(clock) {
  DCHECK(multidevice_setup_client_);
  DCHECK(device_sync_client_);
  DCHECK(secure_channel_client_);
  DCHECK(timer_);
  DCHECK(metrics_recorder_);
}

ConnectionManagerImpl::~ConnectionManagerImpl() {
  metrics_recorder_.reset();
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

bool ConnectionManagerImpl::AttemptNearbyConnection() {
  if (GetStatus() != Status::kDisconnected) {
    PA_LOG(WARNING) << "Connection to host already established or is "
                    << "currently attempting to establish, exiting "
                    << "AttemptConnection().";
    return false;
  }

  const std::optional<multidevice::RemoteDeviceRef> remote_device =
      multidevice_setup_client_->GetHostStatus().second;
  const std::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();

  if (!remote_device || !local_device) {
    PA_LOG(ERROR) << "AttemptConnection() failed because either remote or "
                  << "local device is null.";
    return false;
  }

  connection_attempt_ = secure_channel_client_->InitiateConnectionToDevice(
      *remote_device, *local_device, feature_name_,
      ConnectionMedium::kNearbyConnections, ConnectionPriority::kMedium,
      secure_channel_structured_metrics_logger_);
  connection_attempt_->SetDelegate(this);

  PA_LOG(INFO) << "ConnectionManager status updated to: " << GetStatus();
  OnStatusChanged();

  timer_->Start(FROM_HERE, kConnectionTimeout,
                base::BindOnce(&ConnectionManagerImpl::OnConnectionTimeout,
                               weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void ConnectionManagerImpl::Disconnect() {
  PA_LOG(INFO) << "ConnectionManager disconnecting connection.";
  if (last_status_ == Status::kConnecting) {
    metrics_recorder_->RecordConnectionFailure(
        mojom::ConnectionAttemptFailureReason::CONNECTION_CANCELLED);
  }
  TearDownConnection();
}

void ConnectionManagerImpl::SendMessage(const std::string& payload) {
  if (!channel_) {
    PA_LOG(ERROR) << "SendMessage() failed because channel is null.";
    return;
  }

  channel_->SendMessage(payload, base::DoNothing());
}

void ConnectionManagerImpl::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
        file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  if (!channel_) {
    PA_LOG(ERROR) << "RegisterPayloadFile() failed because channel is null.";
    std::move(registration_result_callback).Run(/*success=*/false);
    return;
  }

  channel_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                std::move(file_transfer_update_callback),
                                std::move(registration_result_callback));
}

void ConnectionManagerImpl::GetHostLastSeenTimestamp(
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  const std::optional<multidevice::RemoteDeviceRef> remote_device =
      multidevice_setup_client_->GetHostStatus().second;
  if (!remote_device) {
    std::move(callback).Run(/*timestamp=*/std::nullopt);
    return;
  }

  secure_channel_client_->GetLastSeenTimestamp(remote_device->GetDeviceId(),
                                               std::move(callback));
}

void ConnectionManagerImpl::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(WARNING) << "AttemptConnection() failed to establish connection with "
                  << "error: " << reason << ".";
  timer_->Stop();
  connection_attempt_.reset();
  if (secure_channel_structured_metrics_logger_) {
    secure_channel_structured_metrics_logger_->UnbindReceiver();
  }
  metrics_recorder_->RecordConnectionFailure(reason);
  OnStatusChanged();
}

void ConnectionManagerImpl::OnConnection(
    std::unique_ptr<ClientChannel> channel) {
  PA_LOG(VERBOSE) << "AttemptConnection() successfully established a "
                  << "connection between local and remote device.";
  timer_->Stop();
  channel_ = std::move(channel);
  channel_->AddObserver(this);
  if (last_status_ == Status::kConnecting) {
    metrics_recorder_->RecordConnectionSuccess(clock_->Now() -
                                               status_change_timestamp_);
  }

  OnStatusChanged();
}

void ConnectionManagerImpl::OnDisconnected() {
  TearDownConnection();
}

void ConnectionManagerImpl::OnMessageReceived(const std::string& payload) {
  NotifyMessageReceived(payload);
}

void ConnectionManagerImpl::OnNearbyConnectionStateChagned(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  if (secure_channel_structured_metrics_logger_) {
    secure_channel_structured_metrics_logger_->LogNearbyConnectionState(step,
                                                                        result);
  }
}

void ConnectionManagerImpl::OnConnectionTimeout() {
  PA_LOG(WARNING) << "AttemptConnection() has timed out. Closing connection "
                  << "attempt.";

  if (secure_channel_structured_metrics_logger_) {
    secure_channel_structured_metrics_logger_->LogDiscoveryAttempt(
        mojom::DiscoveryResult::kFailure, mojom::DiscoveryErrorCode::kTimeout);
  }
  OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason::TIMEOUT_FINDING_DEVICE);
}

void ConnectionManagerImpl::TearDownConnection() {
  // Stop timer in case we are disconnected before the connection timed out.
  timer_->Stop();
  connection_attempt_.reset();
  if (secure_channel_structured_metrics_logger_) {
    secure_channel_structured_metrics_logger_->UnbindReceiver();
  }
  if (channel_)
    channel_->RemoveObserver(this);
  channel_.reset();
  if (last_status_ == Status::kConnected) {
    metrics_recorder_->RecordConnectionDuration(clock_->Now() -
                                                status_change_timestamp_);
  }
  OnStatusChanged();
}

void ConnectionManagerImpl::OnStatusChanged() {
  NotifyStatusChanged();

  Status status = GetStatus();
  if (last_status_ != status) {
    status_change_timestamp_ = clock_->Now();
    last_status_ = status;
  }
}

}  // namespace ash::secure_channel
