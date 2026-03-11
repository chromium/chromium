// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_MANAGER_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/private_ai/error_code.h"

namespace private_ai {

class Connection;
class ConnectionFactory;
class PrivateAiLogger;

// Class for managing the connection to the Private AI server.
class ConnectionManager {
 public:
  ConnectionManager(std::unique_ptr<ConnectionFactory> connection_factory,
                    PrivateAiLogger* logger);
  ~ConnectionManager();

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;

  // Returns the existing connection or creates a new one if it doesn't
  // exist.
  Connection* GetConnection();

 private:
  void OnConnectionDisconnected(ErrorCode error_code);

  // Destroy a connection that is pending destruction.
  //
  // Must be called from a scheduled task.
  void DestroyConnectionPendingDestruction(Connection* connection);

  const raw_ptr<PrivateAiLogger> logger_;
  std::unique_ptr<ConnectionFactory> connection_factory_;

  std::unique_ptr<Connection> connection_;
  // When `connection_` is disconnected, a two-step destruction process is used
  // to propagate an error code through all connection layers, resolve pending
  // callbacks, and avoid side effects on the connection state (i.e. connection
  // destruction) during connection code execution given that the connection
  // code be executed after `on_disconnect` callback.
  //
  // On the first step, the connection is added to pending destructions map and
  // a task is scheduled to destroy that connection.
  //
  // On the second step, the connection destruction is triggered by either a
  // posted task or the connection manager's destructor.
  //
  // This way we ensure that the connection never outlives the connection
  // manager and new requests will be immediately redirected to the new
  // connection.
  base::flat_map<Connection*, std::unique_ptr<Connection>>
      connections_pending_destruction_;

  base::WeakPtrFactory<ConnectionManager> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_MANAGER_H_
