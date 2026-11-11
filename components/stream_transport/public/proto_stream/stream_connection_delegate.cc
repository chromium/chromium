// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/proto_stream/stream_connection_delegate.h"

#include <utility>

#include "services/network/public/cpp/resource_request.h"

namespace stream_transport {

bool StreamConnectionDelegate::ShouldRetryOnHttpFailure(int response_code) {
  return false;
}

std::optional<StreamUploadBody>
StreamConnectionDelegate::GetConnectionRequestBody() {
  return std::nullopt;
}

void DefaultStreamConnectionDelegate::PrepareRequest(
    std::unique_ptr<network::ResourceRequest> request,
    PrepareRequestCallback callback) {
  std::move(callback).Run(std::move(request));
}

}  // namespace stream_transport
