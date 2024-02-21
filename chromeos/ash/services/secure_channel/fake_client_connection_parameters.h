// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CLIENT_CONNECTION_PARAMETERS_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CLIENT_CONNECTION_PARAMETERS_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Test ClientConnectionParameters implementation.
class FakeClientConnectionParameters : public ClientConnectionParameters {
 public:
  FakeClientConnectionParameters(
      const std::string& feature,
      base::OnceCallback<void(const base::UnguessableToken&)>
          destructor_callback =
              base::OnceCallback<void(const base::UnguessableToken&)>());

  FakeClientConnectionParameters(const FakeClientConnectionParameters&) =
      delete;
  FakeClientConnectionParameters& operator=(
      const FakeClientConnectionParameters&) = delete;

  ~FakeClientConnectionParameters() override;

  const std::optional<mojom::ConnectionAttemptFailureReason>& failure_reason() {
    return failure_reason_;
  }

  mojo::Remote<mojom::Channel>& channel() { return channel_; }

  void set_message_receiver(
      std::unique_ptr<mojom::MessageReceiver> message_receiver) {
    message_receiver_ = std::move(message_receiver);
  }

  void set_nearby_connection_state_listener(
      std::unique_ptr<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener) {
    nearby_connection_state_listener_ =
        std::move(nearby_connection_state_listener);
  }

  // If no disconnection has yet occurred, 0 is returned.
  uint32_t disconnection_reason() { return disconnection_reason_; }

  void CancelClientRequest();

 private:
  // ClientConnectionParameters:
  bool HasClientCanceledRequest() override;
  void PerformSetConnectionAttemptFailed(
      mojom::ConnectionAttemptFailureReason reason) override;
  void PerformSetConnectionSucceeded(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
      mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener_receiver) override;

  void UpdateBleDiscoveryState(
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code) override;
  void UpdateNearbyConnectionState(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result) override;
  void UpdateSecureChannelAuthenticationState(
      mojom::SecureChannelState secure_channel_state) override;

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  bool has_canceled_client_request_ = false;

  std::unique_ptr<mojom::MessageReceiver> message_receiver_;
  std::unique_ptr<mojo::Receiver<mojom::MessageReceiver>>
      message_receiver_receiver_;
  std::unique_ptr<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_;
  std::unique_ptr<mojo::Receiver<mojom::NearbyConnectionStateListener>>
      nearby_connection_state_listener_receiver_;

  std::optional<mojom::ConnectionAttemptFailureReason> failure_reason_;

  mojom::DiscoveryResult ble_discovery_result_;
  std::optional<mojom::DiscoveryErrorCode> potential_ble_discovery_error_code_;
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
  mojom::SecureChannelState secure_channel_state_;

  mojo::Remote<mojom::Channel> channel_;
  uint32_t disconnection_reason_ = 0u;

  base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback_;

  base::WeakPtrFactory<FakeClientConnectionParameters> weak_ptr_factory_{this};
};

// Test ClientConnectionParameters::Observer implementation.
class FakeClientConnectionParametersObserver
    : public ClientConnectionParameters::Observer {
 public:
  FakeClientConnectionParametersObserver();

  FakeClientConnectionParametersObserver(
      const FakeClientConnectionParametersObserver&) = delete;
  FakeClientConnectionParametersObserver& operator=(
      const FakeClientConnectionParametersObserver&) = delete;

  ~FakeClientConnectionParametersObserver() override;

  void set_closure_for_next_callback(
      base::OnceClosure closure_for_next_callback) {
    closure_for_next_callback_ = std::move(closure_for_next_callback);
  }

  bool has_connection_request_been_canceled() {
    return has_connection_request_been_canceled_;
  }

 private:
  // ClientConnectionParameters::Observer:
  void OnConnectionRequestCanceled() override;

  bool has_connection_request_been_canceled_ = false;

  base::OnceClosure closure_for_next_callback_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CLIENT_CONNECTION_PARAMETERS_H_
