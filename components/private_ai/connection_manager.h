// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_MANAGER_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_MANAGER_H_

#include <memory>

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

  const raw_ptr<PrivateAiLogger> logger_;
  std::unique_ptr<Connection> connection_;
  std::unique_ptr<ConnectionFactory> connection_factory_;

  base::WeakPtrFactory<ConnectionManager> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_MANAGER_H_
