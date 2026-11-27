// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_GRPC_SERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_GRPC_SERVER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace policy {
class RemoteCommandsState;
}  // namespace policy

namespace fakedms {

// The gRPC remote commands service of fake_dmserver. An external test process
// uses it to send remote commands and to wait for their results.
class RemoteCommandsGrpcServer {
 public:
  virtual ~RemoteCommandsGrpcServer() = default;

  // Starts the service on `unix_socket_uri`, on the current thread. Crashes if
  // the service cannot start. `remote_commands_state` must not be null, and it
  // must outlive the server.
  static std::unique_ptr<RemoteCommandsGrpcServer> Start(
      const std::string& unix_socket_uri,
      policy::RemoteCommandsState* remote_commands_state);

  // Stops the service, then runs `server_stopped_callback`.
  virtual void Stop(base::OnceClosure server_stopped_callback) = 0;
};

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_GRPC_SERVER_H_
