// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/connection_manager_impl.h"

#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace chromeos {
namespace phonehub {
namespace {
constexpr char kPhoneHubFeatureName[] = "phone_hub";
constexpr base::TimeDelta kConnectionTimeoutSeconds(
    base::TimeDelta::FromSeconds(15u));

void RecordConnectionSuccessMetric(bool success) {
  UMA_HISTOGRAM_BOOLEAN("PhoneHub.Connection.Result", success);
}

}  // namespace

ConnectionManagerImpl::MetricsRecorder::MetricsRecorder(
    ConnectionManager* connection_manager,
    base::Clock* clock)
    : connection_manager_(connection_manager),
      status_(connection_manager->GetStatus()),
      clock_(clock),
      status_change_timestamp_(clock_->Now()) {
  connection_manager_->AddObserver(this);
}

ConnectionManagerImpl::MetricsRecorder::~MetricsRecorder() {
  connection_manager_->RemoveObserver(this);
}

void ConnectionManagerImpl::MetricsRecorder::OnConnectionStatusChanged() {
  const ConnectionManager::Status prev_status = status_;
  status_ = connection_manager_->GetStatus();

  const base::TimeDelta delta = clock_->Now() - status_change_timestamp_;
  status_change_timestamp_ = clock_->Now();

  switch (status_) {
    case ConnectionManager::Status::kConnecting:
      break;

    case ConnectionManager::Status::kDisconnected:
      if (prev_status == ConnectionManager::Status::kConnected) {
        UMA_HISTOGRAM_TIMES("PhoneHub.Connectivity.Duration", delta);
      } else if (prev_status == ConnectionManager::Status::kConnecting) {
        RecordConnectionSuccessMetric(false);
      }
      break;

    case ConnectionManager::Status::kConnected:
      if (prev_status == ConnectionManager::Status::kConnecting) {
        UMA_HISTOGRAM_TIMES("PhoneHub.Connectivity.Latency", delta);
        RecordConnectionSuccessMetric(true);
      }
      break;
  }
}

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::secure_channel::SecureChannelClient* secure_channel_client)
    : ConnectionManagerImpl(multidevice_setup_client,
                            device_sync_client,
                            secure_channel_client,
                            std::make_unique<base::OneShotTimer>(),
                            base::DefaultClock::GetInstance()) {}

ConnectionManagerImpl::ConnectionManagerImpl(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    device_sync::DeviceSyncClient* device_sync_client,
    chromeos::secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<base::OneShotTimer> timer,
    base::Clock* clock)
    : multidevice_setup_client_(multidevice_setup_client),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      timer_(std::move(timer)),
      metrics_recorder_(std::make_unique<MetricsRecorder>(this, clock)) {
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

  if (features::IsPhoneHubUseBleEnabled()) {
    connection_attempt_ = secure_channel_client_->ListenForConnectionFromDevice(
        *remote_device, *local_device, kPhoneHubFeatureName,
        secure_channel::ConnectionMedium::kBluetoothLowEnergy,
        secure_channel::ConnectionPriority::kMedium);
  } else {
    connection_attempt_ = secure_channel_client_->InitiateConnectionToDevice(
        *remote_device, *local_device, kPhoneHubFeatureName,
        secure_channel::ConnectionMedium::kNearbyConnections,
        secure_channel::ConnectionPriority::kMedium);
  }
  connection_attempt_->SetDelegate(this);

  PA_LOG(INFO) << "ConnectionManager status updated to: " << GetStatus();
  NotifyStatusChanged();

  timer_->Start(FROM_HERE, kConnectionTimeoutSeconds,
                base::BindOnce(&ConnectionManagerImpl::OnConnectionTimeout,
                               weak_ptr_factory_.GetWeakPtr()));
}

void ConnectionManagerImpl::Disconnect() {
  PA_LOG(INFO) << "ConnectionManager disconnecting connection.";
  TearDownConnection();
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
  timer_->Stop();
  connection_attempt_.reset();
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnConnection(
    std::unique_ptr<chromeos::secure_channel::ClientChannel> channel) {
  PA_LOG(VERBOSE) << "AttemptConnection() successfully established a "
                  << "connection between local and remote device.";
  timer_->Stop();
  channel_ = std::move(channel);
  channel_->AddObserver(this);
  NotifyStatusChanged();
}

void ConnectionManagerImpl::OnDisconnected() {
  TearDownConnection();
}

void ConnectionManagerImpl::OnMessageReceived(const std::string& payload) {
  NotifyMessageReceived(payload);
}

void ConnectionManagerImpl::OnConnectionTimeout() {
  PA_LOG(WARNING) << "AttemptConnection() has timed out. Closing connection "
                  << "attempt.";

  connection_attempt_.reset();
  NotifyStatusChanged();
}

void ConnectionManagerImpl::TearDownConnection() {
  // Stop timer in case we are disconnected before the connection timed out.
  timer_->Stop();
  connection_attempt_.reset();
  if (channel_)
    channel_->RemoveObserver(this);
  channel_.reset();
  NotifyStatusChanged();
}

}  // namespace phonehub
}  // namespace chromeos
