// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/handler_impl.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/private_ai/attestation/server_evidence.h"
#include "components/private_ai/attestation/server_verification_key.h"
#include "components/private_ai/attestation/verification_key_utils.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/err.h"

namespace private_ai {

AttestationHandlerImpl::AttestationHandlerImpl(PrivateAiLogger* logger)
    : logger_(logger),
      verification_keys_(LoadVerificationKeys(GetServerVerificationKey())) {}

AttestationHandlerImpl::AttestationHandlerImpl(
    PrivateAiLogger* logger,
    std::map<uint32_t, VerificationKey> verification_keys)
    : logger_(logger), verification_keys_(std::move(verification_keys)) {}

AttestationHandlerImpl::~AttestationHandlerImpl() = default;

std::optional<oak::session::v1::AttestRequest>
AttestationHandlerImpl::GetAttestationRequest() {
  // For now, this is a placeholder that sends an empty attestation request.
  return oak::session::v1::AttestRequest();
}

bool AttestationHandlerImpl::VerifyAttestationResponse(
    const AttestationEvidence& evidence) {
  if (verification_keys_.empty()) {
    logger_->LogError(FROM_HERE, "No valid verification keys loaded.");
    return false;
  }

  if (evidence.endorsed_evidence.empty()) {
    logger_->LogError(FROM_HERE, "No endorsed evidence found.");
    return false;
  }

  for (const auto& [id, endorsed_evidence] : evidence.endorsed_evidence) {
    if (endorsed_evidence.endorsements.empty()) {
      logger_->LogError(
          FROM_HERE,
          base::StringPrintf("No endorsements found for id: %s", id.c_str()));
      return false;
    }

    for (const auto& endorsement : endorsed_evidence.endorsements) {
      auto parsed_signature = ParseTinkSignature(endorsement.signature);
      if (!parsed_signature) {
        logger_->LogError(FROM_HERE, "Failed to parse Tink signature.");
        return false;
      }

      const auto& [key_id, raw_signature] = *parsed_signature;

      auto key_it = verification_keys_.find(key_id);
      if (key_it == verification_keys_.end()) {
        logger_->LogError(
            FROM_HERE,
            base::StringPrintf("Verification key not found for key ID: %u",
                               key_id));
        return false;
      }

      const VerificationKey& verification_key = key_it->second;
      crypto::SignatureVerifier verifier;

      ERR_clear_error();  // Clear any pre-existing errors from the queue.

      if (!verifier.VerifyInit(
              verification_key.algorithm, raw_signature,
              base::as_bytes(base::span(verification_key.public_key)))) {
        logger_->LogError(FROM_HERE, "SignatureVerifier::VerifyInit failed.");
        uint32_t err = ERR_get_error();  // Get the most recent error.
        if (err != 0) {
          char buf[256];
          ERR_error_string_n(err, buf, sizeof(buf));
          logger_->LogError(
              FROM_HERE,
              base::StringPrintf("VerifyInit BoringSSL error: %s", buf));
        }
        return false;
      }

      // Handle LEGACY prefix side effect where a null byte is appended to the
      // data to be signed.
      if (verification_key.output_prefix_type == OutputPrefixType::LEGACY) {
        std::vector<uint8_t> message_with_null = endorsement.message;
        message_with_null.push_back(0x00);
        verifier.VerifyUpdate(base::as_bytes(base::span(message_with_null)));
      } else {
        verifier.VerifyUpdate(endorsement.message);
      }

      if (!verifier.VerifyFinal()) {
        logger_->LogError(
            FROM_HERE,
            base::StringPrintf("Signature verification failed for key ID: %u",
                               key_id));
        return false;
      }
    }
  }

  return true;
}

}  // namespace private_ai
