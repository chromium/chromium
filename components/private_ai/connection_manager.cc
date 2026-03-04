// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/connection_factory.h"

namespace private_ai {

ConnectionManager::ConnectionManager(
    std::unique_ptr<ConnectionFactory> connection_factory,
    PrivateAiLogger* logger)
    : logger_(logger), connection_factory_(std::move(connection_factory)) {
  CHECK(logger_);
  CHECK(connection_factory_);
}

ConnectionManager::~ConnectionManager() {
  if (connection_) {
    connection_->OnDestroy(ErrorCode::kDestroyed);
  }
}

Connection* ConnectionManager::GetConnection() {
  if (!connection_) {
    connection_ = connection_factory_->Create(
        base::BindOnce(&ConnectionManager::OnConnectionDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  return connection_.get();
}

void ConnectionManager::OnConnectionDisconnected(ErrorCode error_code) {
  CHECK(connection_);
  logger_->LogInfo(
      FROM_HERE, "Connection disconnected. Destroying connection with error: " +
                     base::ToString(error_code));

  // Remove the reference to this Connection object to ensure that any
  // attempt at sending new requests from response handlers will create
  // a new Connection.
  auto connection = std::move(connection_);
  connection->OnDestroy(error_code);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<Connection> connection) {
                       // Release the connection asynchronously to avoid
                       // use-after-free inside this callback.
                     },
                     std::move(connection)));
}

}  // namespace private_ai
