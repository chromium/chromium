// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_ATTESTATION_HANDLER_H_
#define COMPONENTS_LEGION_ATTESTATION_HANDLER_H_

#include <optional>

namespace oak::session::v1 {
class AttestResponse;
class AttestRequest;
}  // namespace oak::session::v1

namespace legion {

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
      const oak::session::v1::AttestResponse& evidence) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_ATTESTATION_HANDLER_H_
