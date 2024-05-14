// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"

#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"

namespace ash::secure_channel {

ConnectionAttempt::Delegate::~Delegate() = default;

ConnectionAttempt::ConnectionAttempt() = default;

ConnectionAttempt::~ConnectionAttempt() = default;

void ConnectionAttempt::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void ConnectionAttempt::NotifyConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  if (delegate_) {
    delegate_->OnConnectionAttemptFailure(reason);
  } else {
    PA_LOG(ERROR) << "NotifyConnectionAttemptFailure: No delegate added.";
    NOTREACHED_IN_MIGRATION();
  }
}

void ConnectionAttempt::NotifyConnection(
    std::unique_ptr<ClientChannel> channel) {
  if (delegate_) {
    delegate_->OnConnection(std::move(channel));
  } else {
    PA_LOG(ERROR) << "NotifyConnection: No delegate added.";
    NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace ash::secure_channel
