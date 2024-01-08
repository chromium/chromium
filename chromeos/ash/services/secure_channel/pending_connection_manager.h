// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace ash::secure_channel {

class AuthenticatedChannel;
class ClientConnectionParameters;
class ConnectionAttemptDetails;
class ConnectionDetails;
enum class ConnectionPriority;

// Attempts to create connections to remote devices. If a connection request
// fails or is canceled, the client will be notified. If a connection is
// created successfully, PendingConnectionManager notifies its delegate.
class PendingConnectionManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnConnection(
        std::unique_ptr<AuthenticatedChannel> authenticated_channel,
        std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
        const ConnectionDetails& connection_details) = 0;
  };

  PendingConnectionManager(const PendingConnectionManager&) = delete;
  PendingConnectionManager& operator=(const PendingConnectionManager&) = delete;

  virtual ~PendingConnectionManager();

  // Attempts a connection according to the provided parameters. If other
  // clients have requested a connection with the same details, a single
  // connection attempt is created which combines all clients which would like
  // to connect to the same device.
  virtual void HandleConnectionRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority) = 0;

 protected:
  PendingConnectionManager(Delegate* delegate);

  void NotifyOnConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
      const ConnectionDetails& connection_details);

 private:
  raw_ptr<Delegate> delegate_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_H_
