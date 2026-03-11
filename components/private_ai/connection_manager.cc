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

  // Move the active connection to the pending destruction list and call
  // `OnDestroy()` to ensure error_code is propagated to all pending callbacks.
  Connection* connection_ptr = connection_.get();
  connection_ptr->OnDestroy(error_code);
  connections_pending_destruction_.emplace(connection_ptr,
                                           std::move(connection_));

  // Schedule destruction of the connection.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ConnectionManager::DestroyConnectionPendingDestruction,
                     weak_factory_.GetWeakPtr(), connection_ptr));
}

void ConnectionManager::DestroyConnectionPendingDestruction(
    Connection* connection) {
  connections_pending_destruction_.erase(connection);
}

}  // namespace private_ai
