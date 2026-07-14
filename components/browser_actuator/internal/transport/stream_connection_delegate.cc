// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"

#include <utility>

#include "services/network/public/cpp/resource_request.h"

namespace browser_actuator {

bool StreamConnectionDelegate::ShouldRetryOnHttpFailure(int response_code) {
  return false;
}

void DefaultStreamConnectionDelegate::PrepareRequest(
    std::unique_ptr<network::ResourceRequest> request,
    PrepareRequestCallback callback) {
  std::move(callback).Run(std::move(request));
}

}  // namespace browser_actuator
