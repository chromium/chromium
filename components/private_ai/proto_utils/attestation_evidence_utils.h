// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PROTO_UTILS_ATTESTATION_EVIDENCE_UTILS_H_
#define COMPONENTS_PRIVATE_AI_PROTO_UTILS_ATTESTATION_EVIDENCE_UTILS_H_

#include <optional>

namespace oak::session::v1 {
class AttestResponse;
}  // namespace oak::session::v1

namespace private_ai {

struct AttestationEvidence;

// Converts oak::session::v1::AttestResponse proto into AttestationEvidence.
std::optional<AttestationEvidence> ConvertToAttestationEvidence(
    const oak::session::v1::AttestResponse& response);

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PROTO_UTILS_ATTESTATION_EVIDENCE_UTILS_H_
