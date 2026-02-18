// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/attestation_evidence_utils.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "components/private_ai/attestation/server_evidence.h"
#include "third_party/oak/chromium/proto/attestation/endorsement.pb.h"
#include "third_party/oak/chromium/proto/crypto/certificate.pb.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "third_party/oak/chromium/proto/variant.pb.h"

namespace private_ai {

std::optional<AttestationEvidence> ConvertToAttestationEvidence(
    const oak::session::v1::AttestResponse& response) {
  AttestationEvidence output;
  for (const auto& [key, value] : response.endorsed_evidence()) {
    if (!value.has_endorsements()) {
      return std::nullopt;
    }

    std::vector<Endorsement> endorsements_list;
    for (const auto& event_variant : value.endorsements().events()) {
      oak::attestation::v1::SessionBindingPublicKeyEndorsement
          session_binding_endorsement;
      if (!session_binding_endorsement.ParseFromString(event_variant.value())) {
        // This is not the endorsement we are looking for.
        continue;
      }

      const auto& certificate =
          session_binding_endorsement.ca_endorsement().certificate();
      const std::string& message = certificate.serialized_payload();
      const std::string& signature = certificate.signature_info().signature();

      Endorsement endorsement;
      endorsement.message = base::ToVector(base::as_byte_span(message));
      endorsement.signature = base::ToVector(base::as_byte_span(signature));
      endorsements_list.push_back(std::move(endorsement));
    }

    if (!endorsements_list.empty()) {
      output.endorsed_evidence[key].endorsements = std::move(endorsements_list);
    }
  }

  return output;
}

}  // namespace private_ai
