// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ATTESTATION_HANDLER_H_
#define COMPONENTS_PRIVATE_AI_ATTESTATION_HANDLER_H_

#include <optional>

#include "components/private_ai/attestation/server_evidence.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace private_ai {

// Interface for handling attestation-related operations.
class AttestationHandler {
 public:
  virtual ~AttestationHandler() = default;

  // Generates the initial attestation request message to send to the service.
  // Returns std::nullopt on failure.
  virtual std::optional<oak::session::v1::AttestRequest>
  GetAttestationRequest() = 0;

  // Verifies the attestation evidence received from the server
  // in response to the attestation request.
  // Returns true if the attestation is valid, false otherwise.
  virtual bool VerifyAttestationResponse(
      const AttestationEvidence& evidence) = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ATTESTATION_HANDLER_H_
