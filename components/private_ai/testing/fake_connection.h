// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_CONNECTION_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_CONNECTION_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/legion.pb.h"

namespace private_ai {

class FakeConnection : public Connection {
 public:
  struct PendingRequest {
    PendingRequest();

    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    PendingRequest(const PendingRequest&) = delete;
    PendingRequest& operator=(const PendingRequest&) = delete;

    ~PendingRequest();

    proto::LegionRequest request;
    base::TimeDelta timeout;
    OnRequestCallback callback;
  };

  explicit FakeConnection(base::OnceClosure on_disconnect);
  ~FakeConnection() override;

  // Connection implementation:
  void Send(proto::LegionRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  // Resolves all pending callbacks with ErrorCode::kNetworkError and runs the
  // on_disconnect callback.
  void SimulateDisconnect();

  std::vector<PendingRequest>& pending_requests() { return pending_requests_; }

 private:
  base::OnceClosure on_disconnect_;
  std::vector<PendingRequest> pending_requests_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_CONNECTION_H_
