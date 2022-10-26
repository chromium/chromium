// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_DELEGATE_H_

#include <ostream>

#include "base/unguessable_token.h"

namespace ash::secure_channel {

class PendingConnectionRequestDelegate {
 public:
  enum class FailedConnectionReason {
    // The requester of the connection canceled the attempt before it resulted
    // in a connection.
    kRequestCanceledByClient,

    // The request could not be completed due to an issue establishing a
    // connection (e.g., timeout finding device to remote device).
    kRequestFailed
  };

  PendingConnectionRequestDelegate();

  PendingConnectionRequestDelegate(const PendingConnectionRequestDelegate&) =
      delete;
  PendingConnectionRequestDelegate& operator=(
      const PendingConnectionRequestDelegate&) = delete;

  virtual ~PendingConnectionRequestDelegate();

  // Invoked when a PendingConnectionRequest fails to establish a connection.
  // |request_id| corresponds to the ID returned by
  // PendingConnectionRequest::request_id().
  virtual void OnRequestFinishedWithoutConnection(
      const base::UnguessableToken& request_id,
      FailedConnectionReason reason) = 0;
};

std::ostream& operator<<(
    std::ostream& stream,
    const PendingConnectionRequestDelegate::FailedConnectionReason& reason);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_DELEGATE_H_
