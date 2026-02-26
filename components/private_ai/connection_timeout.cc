// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_timeout.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/connection.h"

namespace private_ai {

ConnectionTimeout::ConnectionTimeout(
    std::unique_ptr<Connection> inner_connection)
    : inner_connection_(std::move(inner_connection)) {
  CHECK(inner_connection_);
}

ConnectionTimeout::~ConnectionTimeout() = default;

void ConnectionTimeout::Send(proto::PrivateAiRequest request,
                             base::TimeDelta timeout,
                             OnRequestCallback callback) {
  const int32_t internal_request_id = next_internal_request_id_++;
  pending_callbacks_.emplace(internal_request_id, std::move(callback));

  inner_connection_->Send(
      std::move(request), timeout,
      base::BindOnce(&ConnectionTimeout::OnResponse, weak_factory_.GetWeakPtr(),
                     internal_request_id));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ConnectionTimeout::OnResponse, weak_factory_.GetWeakPtr(),
                     internal_request_id,
                     base::unexpected(ErrorCode::kTimeout)),
      timeout);
}

void ConnectionTimeout::OnDestroy(ErrorCode error) {
  auto pending = std::move(pending_callbacks_);
  for (auto& entry : pending) {
    std::move(entry.second).Run(base::unexpected(error));
  }

  inner_connection_->OnDestroy(error);

  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionTimeout::OnResponse(
    int32_t internal_request_id,
    base::expected<proto::PrivateAiResponse, ErrorCode> result) {
  auto it = pending_callbacks_.find(internal_request_id);
  if (it == pending_callbacks_.end()) {
    return;
  }

  auto callback = std::move(it->second);
  pending_callbacks_.erase(it);
  std::move(callback).Run(std::move(result));
}

}  // namespace private_ai
