// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_DELEGATE_H_

#include <string>

#include "chromeos/ash/services/secure_channel/connection_attempt_delegate.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::secure_channel {

class AuthenticatedChannel;

class FakeConnectionAttemptDelegate : public ConnectionAttemptDelegate {
 public:
  FakeConnectionAttemptDelegate();

  FakeConnectionAttemptDelegate(const FakeConnectionAttemptDelegate&) = delete;
  FakeConnectionAttemptDelegate& operator=(
      const FakeConnectionAttemptDelegate&) = delete;

  ~FakeConnectionAttemptDelegate() override;

  const AuthenticatedChannel* authenticated_channel() const {
    return authenticated_channel_.get();
  }

  const absl::optional<ConnectionDetails>& connection_details() const {
    return connection_details_;
  }

  const absl::optional<ConnectionAttemptDetails>& connection_attempt_details()
      const {
    return connection_attempt_details_;
  }

 private:
  // ConnectionAttemptDelegate:
  void OnConnectionAttemptSucceeded(
      const ConnectionDetails& connection_details,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) override;
  void OnConnectionAttemptFinishedWithoutConnection(
      const ConnectionAttemptDetails& connection_attempt_details) override;

  absl::optional<ConnectionAttemptDetails> connection_attempt_details_;
  absl::optional<ConnectionDetails> connection_details_;
  std::unique_ptr<AuthenticatedChannel> authenticated_channel_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_DELEGATE_H_
