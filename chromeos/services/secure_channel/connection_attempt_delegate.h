// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_DELEGATE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_DELEGATE_H_

#include <string>
#include <vector>

#include "chromeos/services/secure_channel/connection_attempt_details.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/device_id_pair.h"

namespace chromeos {

namespace secure_channel {

class AuthenticatedChannel;

class ConnectionAttemptDelegate {
 public:
  ConnectionAttemptDelegate() = default;

  ConnectionAttemptDelegate(const ConnectionAttemptDelegate&) = delete;
  ConnectionAttemptDelegate& operator=(const ConnectionAttemptDelegate&) =
      delete;

  virtual ~ConnectionAttemptDelegate() = default;

  // Invoked when a ConnectionAttempt has successfully resulted in a connection.
  // |attempt_id| corresponds to the ID returned by
  // ConnectionAttempt::attempt_id().
  virtual void OnConnectionAttemptSucceeded(
      const ConnectionDetails& connection_details,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) = 0;

  // Invoked when a ConnectionAttempt has finished without achieving a
  // connection to the remote device (due to cancellation by the client or
  // connection failures). |attempt_id| corresponds to the ID returned by
  // ConnectionAttempt::attempt_id().
  virtual void OnConnectionAttemptFinishedWithoutConnection(
      const ConnectionAttemptDetails& connection_attempt_details) = 0;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_DELEGATE_H_
