// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/fake_connection.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

FakeConnection::PendingRequest::PendingRequest() = default;

FakeConnection::PendingRequest::PendingRequest(PendingRequest&&) = default;

FakeConnection::PendingRequest& FakeConnection::PendingRequest::operator=(
    PendingRequest&&) = default;

FakeConnection::PendingRequest::~PendingRequest() = default;

FakeConnection::FakeConnection(
    base::OnceCallback<void(ErrorCode)> on_disconnect,
    base::OnceClosure on_destruction)
    : on_disconnect_(std::move(on_disconnect)),
      on_destruction_(std::move(on_destruction)) {}

FakeConnection::~FakeConnection() {
  if (on_destruction_) {
    std::move(on_destruction_).Run();
  }
}

void FakeConnection::Send(proto::PrivateAiRequest request,
                          base::TimeDelta timeout,
                          OnRequestCallback callback) {
  CHECK(callback);
  PendingRequest pending_request;
  pending_request.request = std::move(request);
  pending_request.timeout = timeout;
  pending_request.callback = std::move(callback);

  pending_requests_.push_back(std::move(pending_request));
}

void FakeConnection::OnDestroy(ErrorCode error) {
  on_disconnect_.Reset();
  auto callbacks = std::move(pending_requests_);
  for (auto& pending_request : callbacks) {
    std::move(pending_request.callback).Run(base::unexpected(error));
  }
}

void FakeConnection::SimulateDisconnect() {
  if (on_disconnect_) {
    std::move(on_disconnect_).Run(ErrorCode::kNetworkError);
  }
}

}  // namespace private_ai
