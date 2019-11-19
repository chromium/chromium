// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CLIENT_CONNECTION_PARAMETERS_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CLIENT_CONNECTION_PARAMETERS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Test ClientConnectionParameters implementation.
class FakeClientConnectionParameters : public ClientConnectionParameters {
 public:
  FakeClientConnectionParameters(
      const std::string& feature,
      base::OnceCallback<void(const base::UnguessableToken&)>
          destructor_callback =
              base::OnceCallback<void(const base::UnguessableToken&)>());

  ~FakeClientConnectionParameters() override;

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  failure_reason() {
    return failure_reason_;
  }

  mojo::Remote<mojom::Channel>& channel() { return channel_; }

  void set_message_receiver(
      std::unique_ptr<mojom::MessageReceiver> message_receiver) {
    message_receiver_ = std::move(message_receiver);
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
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver)
      override;

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  bool has_canceled_client_request_ = false;

  std::unique_ptr<mojom::MessageReceiver> message_receiver_;
  std::unique_ptr<mojo::Receiver<mojom::MessageReceiver>>
      message_receiver_receiver_;

  base::Optional<mojom::ConnectionAttemptFailureReason> failure_reason_;

  mojo::Remote<mojom::Channel> channel_;
  uint32_t disconnection_reason_ = 0u;

  base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback_;

  base::WeakPtrFactory<FakeClientConnectionParameters> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeClientConnectionParameters);
};

// Test ClientConnectionParameters::Observer implementation.
class FakeClientConnectionParametersObserver
    : public ClientConnectionParameters::Observer {
 public:
  FakeClientConnectionParametersObserver();
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

  DISALLOW_COPY_AND_ASSIGN(FakeClientConnectionParametersObserver);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CLIENT_CONNECTION_PARAMETERS_H_
