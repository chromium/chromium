// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONNECTION_METRICS_H_
#define COMPONENTS_PRIVATE_AI_CONNECTION_METRICS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/private_ai/connection.h"

namespace private_ai {

// A decorator for `Connection` that records metrics for `Send` calls.
// It wraps the provided `inner_connection` and records request/response
// sizes, latencies, and error codes.
class ConnectionMetrics : public Connection {
 public:
  explicit ConnectionMetrics(std::unique_ptr<Connection> inner_connection);
  ~ConnectionMetrics() override;

  ConnectionMetrics(const ConnectionMetrics&) = delete;
  ConnectionMetrics& operator=(const ConnectionMetrics&) = delete;

  // Connection override:
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(ErrorCode error) override;

 private:
  void OnResponse(base::TimeTicks start_time,
                  OnRequestCallback callback,
                  base::expected<proto::PrivateAiResponse, ErrorCode> result);

  std::unique_ptr<Connection> inner_connection_;

  base::WeakPtrFactory<ConnectionMetrics> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONNECTION_METRICS_H_
