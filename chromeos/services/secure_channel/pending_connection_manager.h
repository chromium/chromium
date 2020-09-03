// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/connection_attempt_details.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_medium.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

class AuthenticatedChannel;

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
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PendingConnectionManager);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_H_
