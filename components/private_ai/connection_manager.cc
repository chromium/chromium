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
#include "components/private_ai/status_code.h"

namespace private_ai {

ConnectionManager::ConnectionManager(
    std::unique_ptr<ConnectionFactory> connection_factory,
    PrivateAiLogger* logger)
    : logger_(logger), connection_factory_(std::move(connection_factory)) {
  CHECK(logger_);
  CHECK(connection_factory_);
}

ConnectionManager::~ConnectionManager() {
  for (auto& [feature_name, state] : connections_) {
    if (state.connection) {
      state.connection->OnDestroy(StatusCode::kDestroyed);
    }
  }
}

Connection* ConnectionManager::GetConnection(proto::FeatureName feature_name) {
  auto& state = connections_[feature_name];
  if (!state.connection) {
    state.connection_id++;
    state.connection = connection_factory_->Create(base::BindRepeating(
        &ConnectionManager::OnConnectionDisconnected,
        weak_factory_.GetWeakPtr(), feature_name, state.connection_id));
  }
  return state.connection.get();
}

void ConnectionManager::OnConnectionDisconnected(
    proto::FeatureName feature_name,
    int connection_id,
    StatusCode status_code) {
  auto it = connections_.find(feature_name);
  if (it == connections_.end()) {
    return;
  }
  auto& state = it->second;
  if (connection_id != state.connection_id || !state.connection) {
    return;
  }

  logger_->LogInfo(FROM_HERE,
                   status_code == StatusCode::kUnusedConnection
                       ? "Closing unused connection for feature: " +
                             base::ToString(static_cast<int>(feature_name))
                       : "Connection disconnected for feature: " +
                             base::ToString(static_cast<int>(feature_name)) +
                             ". Destroying connection with status: " +
                             base::ToString(status_code));

  // Move the active connection to the pending destruction list and call
  // `OnDestroy()` to ensure status is propagated to all pending callbacks.
  Connection* connection_ptr = state.connection.get();
  connection_ptr->OnDestroy(status_code);
  connections_pending_destruction_.emplace(connection_ptr,
                                           std::move(state.connection));

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
