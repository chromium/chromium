// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/resume_body_connection_delegate.h"

#include <utility>

#include "base/check.h"
#include "services/network/public/cpp/resource_request.h"

namespace browser_actuator {

namespace {
// OnePlatform reads the binary WatchSessionsRequest from the request body.
constexpr char kProtoContentType[] = "application/x-protobuf";
}  // namespace

ResumeBodyConnectionDelegate::ResumeBodyConnectionDelegate(
    BodyProvider body_provider,
    std::unique_ptr<StreamConnectionDelegate> inner)
    : body_provider_(std::move(body_provider)), inner_(std::move(inner)) {
  CHECK(body_provider_);
  CHECK(inner_);
}

ResumeBodyConnectionDelegate::~ResumeBodyConnectionDelegate() = default;

void ResumeBodyConnectionDelegate::PrepareRequest(
    std::unique_ptr<network::ResourceRequest> request,
    PrepareRequestCallback callback) {
  // The resume state rides the request body (see GetConnectionRequestBody),
  // so there is nothing to add to the request itself.
  inner_->PrepareRequest(std::move(request), std::move(callback));
}

std::optional<StreamUploadBody>
ResumeBodyConnectionDelegate::GetConnectionRequestBody() {
  return StreamUploadBody{body_provider_.Run(), kProtoContentType};
}

void ResumeBodyConnectionDelegate::OnConnectionEstablished() {
  inner_->OnConnectionEstablished();
}

bool ResumeBodyConnectionDelegate::ShouldRetryOnHttpFailure(int response_code) {
  return inner_->ShouldRetryOnHttpFailure(response_code);
}

}  // namespace browser_actuator
