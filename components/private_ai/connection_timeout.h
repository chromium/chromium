// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_TIMEOUT_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_TIMEOUT_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/private_ai/connection.h"

namespace private_ai {

// A decorator for `Connection` that adds timeout handling to `Send` calls.
// It wraps the provided `inner_connection` and ensures that the callback
// is invoked with `ErrorCode::kTimeout` if the response is not received
// within the specified timeout.
class ConnectionTimeout : public Connection {
 public:
  explicit ConnectionTimeout(std::unique_ptr<Connection> inner_connection);
  ~ConnectionTimeout() override;

  ConnectionTimeout(const ConnectionTimeout&) = delete;
  ConnectionTimeout& operator=(const ConnectionTimeout&) = delete;

  // Connection override:
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(ErrorCode error) override;

 private:
  void OnResponse(int32_t internal_request_id,
                  base::expected<proto::PrivateAiResponse, ErrorCode> result);

  std::unique_ptr<Connection> inner_connection_;

  int32_t next_internal_request_id_{1};
  base::flat_map<int32_t, OnRequestCallback> pending_callbacks_;

  base::WeakPtrFactory<ConnectionTimeout> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_TIMEOUT_H_
