// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/client_connection_parameters_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"

namespace ash::secure_channel {

// static
ClientConnectionParametersImpl::Factory*
    ClientConnectionParametersImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ClientConnectionParameters>
ClientConnectionParametersImpl::Factory::Create(
    const std::string& feature,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote,
    mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
        secure_channel_structured_metrics_logger) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        feature, std::move(connection_delegate_remote),
        std::move(secure_channel_structured_metrics_logger));
  }

  return base::WrapUnique(new ClientConnectionParametersImpl(
      feature, std::move(connection_delegate_remote),
      std::move(secure_channel_structured_metrics_logger)));
}

// static
void ClientConnectionParametersImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ClientConnectionParametersImpl::Factory::~Factory() = default;

ClientConnectionParametersImpl::ClientConnectionParametersImpl(
    const std::string& feature,
    mojo::PendingRemote<mojom::ConnectionDelegate> connection_delegate_remote,
    mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
        secure_channel_structured_metrics_logger)
    : ClientConnectionParameters(feature),
      connection_delegate_remote_(std::move(connection_delegate_remote)),
      secure_channel_structured_metrics_logger_remote_(
          std::move(secure_channel_structured_metrics_logger)) {
  // If the client disconnects its delegate, the client is signaling that the
  // connection request has been canceled.
  connection_delegate_remote_.set_disconnect_handler(base::BindOnce(
      &ClientConnectionParametersImpl::OnConnectionDelegateRemoteDisconnected,
      base::Unretained(this)));
}

ClientConnectionParametersImpl::~ClientConnectionParametersImpl() = default;

bool ClientConnectionParametersImpl::HasClientCanceledRequest() {
  return !connection_delegate_remote_.is_connected();
}

void ClientConnectionParametersImpl::PerformSetConnectionAttemptFailed(
    mojom::ConnectionAttemptFailureReason reason) {
  connection_delegate_remote_->OnConnectionAttemptFailure(reason);
}

void ClientConnectionParametersImpl::PerformSetConnectionSucceeded(
    mojo::PendingRemote<mojom::Channel> channel,
    mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
    mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener_receiver) {
  connection_delegate_remote_->OnConnection(
      std::move(channel), std::move(message_receiver_receiver),
      std::move(nearby_connection_state_listener_receiver));
}

void ClientConnectionParametersImpl::OnConnectionDelegateRemoteDisconnected() {
  NotifyConnectionRequestCanceled();
}

void ClientConnectionParametersImpl::UpdateBleDiscoveryState(
    mojom::DiscoveryResult discovery_state,
    std::optional<mojom::DiscoveryErrorCode> potential_error_code) {
  secure_channel_structured_metrics_logger_remote_->LogDiscoveryAttempt(
      discovery_state, potential_error_code);
}

void ClientConnectionParametersImpl::UpdateNearbyConnectionState(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  secure_channel_structured_metrics_logger_remote_->LogNearbyConnectionState(
      step, result);
}

void ClientConnectionParametersImpl::UpdateSecureChannelAuthenticationState(
    mojom::SecureChannelState secure_channel_state) {
  secure_channel_structured_metrics_logger_remote_->LogSecureChannelState(
      secure_channel_state);
}

}  // namespace ash::secure_channel
