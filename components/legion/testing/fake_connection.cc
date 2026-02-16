// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/testing/fake_connection.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/legion/proto/legion.pb.h"

namespace private_ai {

FakeConnection::PendingRequest::PendingRequest() = default;

FakeConnection::PendingRequest::PendingRequest(PendingRequest&&) = default;

FakeConnection::PendingRequest& FakeConnection::PendingRequest::operator=(
    PendingRequest&&) = default;

FakeConnection::PendingRequest::~PendingRequest() = default;

FakeConnection::FakeConnection(base::OnceClosure on_disconnect)
    : on_disconnect_(std::move(on_disconnect)) {}

FakeConnection::~FakeConnection() = default;

void FakeConnection::Send(proto::LegionRequest request,
                          base::TimeDelta timeout,
                          OnRequestCallback callback) {
  PendingRequest pending_request;
  pending_request.request = std::move(request);
  pending_request.timeout = timeout;
  pending_request.callback = std::move(callback);

  pending_requests_.push_back(std::move(pending_request));
}

void FakeConnection::SimulateDisconnect() {
  auto callbacks = std::move(pending_requests_);
  for (auto& pending_request : callbacks) {
    std::move(pending_request.callback)
        .Run(base::unexpected(ErrorCode::kNetworkError));
  }
  std::move(on_disconnect_).Run();
}

}  // namespace private_ai
