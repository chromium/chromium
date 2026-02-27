// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_basic.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

ConnectionBasic::ConnectionBasic(
    std::unique_ptr<SecureChannel::Factory> secure_channel_factory,
    base::OnceCallback<void(ErrorCode)> on_disconnect)
    : on_disconnect_(std::move(on_disconnect)) {
  CHECK(secure_channel_factory);
  CHECK(on_disconnect_);

  secure_channel_ = secure_channel_factory->Create(base::BindRepeating(
      &ConnectionBasic::OnResponseReceived, weak_factory_.GetWeakPtr()));
  CHECK(secure_channel_);
}

ConnectionBasic::~ConnectionBasic() = default;

void ConnectionBasic::Send(proto::PrivateAiRequest request,
                           base::TimeDelta timeout,
                           OnRequestCallback callback) {
  // Indicates that `secure_channel_` is already closed.
  if (!on_disconnect_) {
    std::move(callback).Run(base::unexpected(ErrorCode::kError));
    return;
  }

  const int32_t request_id = next_request_id_++;
  request.set_request_id(request_id);

  pending_request_callbacks_.emplace(request_id, std::move(callback));

  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  if (!secure_channel_->Write(
          Request(serialized_request.begin(), serialized_request.end()))) {
    // The channel is in a permanent failure state.
    DVLOG(1) << "Secure channel write failed.";
    CallOnDisconnect(ErrorCode::kError);
  }
}

void ConnectionBasic::OnDestroy(ErrorCode error) {
  on_disconnect_.Reset();
  auto pending_requests = std::move(pending_request_callbacks_);
  for (auto& pending_request : pending_requests) {
    std::move(pending_request.second).Run(base::unexpected(error));
  }

  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionBasic::OnResponseReceived(
    base::expected<Response, ErrorCode> result) {
  if (!result.has_value()) {
    DVLOG(1) << "Secure channel returned an error.";
    CallOnDisconnect(result.error());
    return;
  }

  proto::PrivateAiResponse private_ai_response;
  if (!private_ai_response.ParseFromArray(result->data(), result->size())) {
    LOG(ERROR) << "Failed to parse PrivateAiResponse";
    // This is a protocol error. We don't know which request this response was
    // for, so we fail all of them.
    CallOnDisconnect(ErrorCode::kResponseParseError);
    return;
  }

  auto it = pending_request_callbacks_.find(private_ai_response.request_id());
  if (it == pending_request_callbacks_.end()) {
    LOG(ERROR) << "Received PrivateAiResponse for unknown request_id: "
               << private_ai_response.request_id();
    return;
  }

  auto callback = std::move(it->second);
  pending_request_callbacks_.erase(it);

  std::move(callback).Run(std::move(private_ai_response));
}

void ConnectionBasic::CallOnDisconnect(ErrorCode error_code) {
  // First reject pending requests so that higher layers like
  // ConnectionTokenAttestation can report the correct error code.
  auto pending_requests = std::move(pending_request_callbacks_);
  for (auto& pending_request : pending_requests) {
    std::move(pending_request.second).Run(base::unexpected(error_code));
  }

  if (on_disconnect_) {
    std::move(on_disconnect_).Run(error_code);
  }
}

}  // namespace private_ai
