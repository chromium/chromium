// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_CONNECTION_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_CONNECTION_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/status_code.h"

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

    proto::PrivateAiRequest request;
    base::TimeDelta timeout;
    OnRequestCallback callback;
  };

  explicit FakeConnection(
      base::RepeatingCallback<void(StatusCode)> on_disconnect,
      base::OnceClosure on_destruction = {});
  ~FakeConnection() override;

  // Connection implementation:
  void Send(proto::PrivateAiRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  void OnDestroy(StatusCode status_code) override;

  // Resolves all pending callbacks with StatusCode::kNetworkError and runs the
  // on_disconnect callback.
  void SimulateDisconnect();

  std::vector<PendingRequest>& pending_requests() { return pending_requests_; }

 private:
  base::RepeatingCallback<void(StatusCode)> on_disconnect_;
  base::OnceClosure on_destruction_;
  std::vector<PendingRequest> pending_requests_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_CONNECTION_H_
