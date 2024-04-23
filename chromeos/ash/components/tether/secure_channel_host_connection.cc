// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/secure_channel_host_connection.h"

namespace ash::tether {

namespace {

secure_channel::ConnectionPriority GetSecureChannelConnectionPriority(
    HostConnection::Factory::ConnectionPriority connection_priority) {
  switch (connection_priority) {
    case HostConnection::Factory::ConnectionPriority::kLow:
      return secure_channel::ConnectionPriority::kLow;
    case HostConnection::Factory::ConnectionPriority::kMedium:
      return secure_channel::ConnectionPriority::kMedium;
    case HostConnection::Factory::ConnectionPriority::kHigh:
      return secure_channel::ConnectionPriority::kHigh;
  }
}
}  // namespace

SecureChannelHostConnection::Factory::Factory(
    raw_ptr<device_sync::DeviceSyncClient> device_sync_client,
    raw_ptr<secure_channel::SecureChannelClient> secure_channel_client,
    raw_ptr<TetherHostFetcher> tether_host_fetcher)
    : device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      tether_host_fetcher_(tether_host_fetcher) {}

SecureChannelHostConnection::Factory::~Factory() = default;

void SecureChannelHostConnection::Factory::ScanForTetherHostAndCreateConnection(
    const std::string& device_id,
    ConnectionPriority connection_priority,
    raw_ptr<PayloadListener> payload_listener,
    OnDisconnectionCallback on_disconnection,
    HostConnection::Factory::OnConnectionCreatedCallback
        on_connection_created) {
  std::optional<multidevice::RemoteDeviceRef> tether_host =
      tether_host_fetcher_->GetTetherHost();

  CHECK(tether_host.has_value());
  CHECK(tether_host->GetDeviceId() == device_id);

  PA_LOG(INFO) << "Creating a SecureChannelHostConnection to "
               << tether_host->GetTruncatedDeviceIdForLogs() << ".";
  return Create(TetherHost(*tether_host), connection_priority, payload_listener,
                std::move(on_disconnection), std::move(on_connection_created));
}

void SecureChannelHostConnection::Factory::Create(
    const TetherHost& tether_host,
    ConnectionPriority connection_priority,
    raw_ptr<PayloadListener> payload_listener,
    OnDisconnectionCallback on_disconnection,
    HostConnection::Factory::OnConnectionCreatedCallback
        on_connection_created) {
  PA_LOG(INFO) << "Attempting to create a SecureChannelHostConnection to "
               << tether_host.GetTruncatedDeviceIdForLogs() << ".";

  CHECK(tether_host.remote_device_ref().has_value());

  std::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();

  CHECK(local_device.has_value());

  base::Uuid connection_attempt_id = base::Uuid::GenerateRandomV4();

  active_connection_attempts_.emplace(
      connection_attempt_id,
      std::make_unique<ConnectionAttemptDelegateImpl>(
          *local_device, *tether_host.remote_device_ref(),
          secure_channel_client_, payload_listener,
          std::move(on_disconnection)));

  active_connection_attempts_.at(connection_attempt_id)
      ->Start(
          GetSecureChannelConnectionPriority(connection_priority),
          base::BindOnce(&Factory::OnConnectionAttemptFinished,
                         weak_ptr_factory_.GetWeakPtr(), connection_attempt_id,
                         std::move(on_connection_created)));
}

SecureChannelHostConnection::Factory::ConnectionAttemptDelegateImpl::
    ConnectionAttemptDelegateImpl(
        const multidevice::RemoteDeviceRef& local_device,
        const multidevice::RemoteDeviceRef& remote_device,
        secure_channel::SecureChannelClient* secure_channel_client,
        HostConnection::PayloadListener* payload_listener,
        HostConnection::OnDisconnectionCallback on_disconnection)
    : local_device_(local_device),
      remote_device_(remote_device),
      payload_listener_(payload_listener),
      on_disconnection_(std::move(on_disconnection)),
      secure_channel_client_(secure_channel_client) {}

SecureChannelHostConnection::Factory::ConnectionAttemptDelegateImpl::
    ~ConnectionAttemptDelegateImpl() = default;

void SecureChannelHostConnection::Factory::ConnectionAttemptDelegateImpl::Start(
    secure_channel::ConnectionPriority connection_priority,
    OnConnectionAttemptDelegateFinishedCallback
        on_connection_attempt_delegate_finished) {
  on_connection_attempt_delegate_finished_ =
      std::move(on_connection_attempt_delegate_finished);
  connection_attempt_ = secure_channel_client_->ListenForConnectionFromDevice(
      remote_device_, local_device_, "magic_tether",
      secure_channel::ConnectionMedium::kBluetoothLowEnergy,
      connection_priority);
  connection_attempt_->SetDelegate(this);
}
// secure_channel::ConnectionAttempt::Delegate:
void SecureChannelHostConnection::Factory::ConnectionAttemptDelegateImpl::
    OnConnectionAttemptFailure(
        secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  PA_LOG(ERROR) << "Failed to connect to "
                << remote_device_.GetTruncatedDeviceIdForLogs()
                << ". Failure Reason: [" << reason << "].";
  std::move(on_connection_attempt_delegate_finished_)
      .Run(/*host_connection=*/nullptr);
}
void SecureChannelHostConnection::Factory::ConnectionAttemptDelegateImpl::
    OnConnection(std::unique_ptr<secure_channel::ClientChannel> channel) {
  std::move(on_connection_attempt_delegate_finished_)
      .Run(base::WrapUnique<HostConnection>(new SecureChannelHostConnection(
          payload_listener_, std::move(on_disconnection_),
          std::move(channel))));
}

void SecureChannelHostConnection::Factory::OnConnectionAttemptFinished(
    base::Uuid connection_attempt_id,
    HostConnection::Factory::OnConnectionCreatedCallback on_connection_created,
    std::unique_ptr<HostConnection> host_connection) {
  active_connection_attempts_.erase(connection_attempt_id);
  std::move(on_connection_created).Run(std::move(host_connection));
}

SecureChannelHostConnection::SecureChannelHostConnection(
    raw_ptr<HostConnection::PayloadListener> payload_listener,
    HostConnection::OnDisconnectionCallback on_disconnection,
    std::unique_ptr<secure_channel::ClientChannel> channel)
    : HostConnection(payload_listener, std::move(on_disconnection)),
      client_channel_(std::move(channel)) {
  client_channel_->AddObserver(this);
}

SecureChannelHostConnection::~SecureChannelHostConnection() {
  client_channel_->RemoveObserver(this);
}

void SecureChannelHostConnection::SendMessage(
    std::unique_ptr<MessageWrapper> message,
    OnMessageSentCallback on_message_sent_callback) {
  client_channel_->SendMessage(message->ToRawMessage(),
                               std::move(on_message_sent_callback));
}

void SecureChannelHostConnection::OnDisconnected() {
  std::move(on_disconnection_).Run();
}

void SecureChannelHostConnection::OnMessageReceived(
    const std::string& message) {
  ParseMessageAndNotifyListener(message);
}

}  // namespace ash::tether
