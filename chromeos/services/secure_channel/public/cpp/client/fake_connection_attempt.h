// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_ATTEMPT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_ATTEMPT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace secure_channel {

// Test double implementation of ConnectionAttemptImpl.
class FakeConnectionAttempt : public ConnectionAttemptImpl {
 public:
  FakeConnectionAttempt();
  ~FakeConnectionAttempt() override;

  using ConnectionAttempt::NotifyConnectionAttemptFailure;
  using ConnectionAttempt::NotifyConnection;

  // ConnectionAttemptImpl:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(mojo::PendingRemote<mojom::Channel> channel,
                    mojo::PendingReceiver<mojom::MessageReceiver>
                        message_receiver_receiver) override;

  void set_on_connection_attempt_failure_callback(base::OnceClosure callback) {
    on_connection_attempt_failure_callback_ = std::move(callback);
  }

  void set_on_connection_callback(base::OnceClosure callback) {
    on_connection_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure on_connection_attempt_failure_callback_;
  base::OnceClosure on_connection_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionAttempt);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_ATTEMPT_H_
