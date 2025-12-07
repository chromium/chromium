// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/attestation_handler_impl.h"

#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

AttestationHandlerImpl::AttestationHandlerImpl() = default;
AttestationHandlerImpl::~AttestationHandlerImpl() = default;

std::optional<oak::session::v1::AttestRequest>
AttestationHandlerImpl::GetAttestationRequest() {
  // For now, this is a placeholder that sends an empty attestation request.
  return oak::session::v1::AttestRequest();
}

bool AttestationHandlerImpl::VerifyAttestationResponse(
    const oak::session::v1::AttestResponse& evidence) {
  // For now, this is a placeholder that assumes any response is valid.
  return true;
}

}  // namespace legion
