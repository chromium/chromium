// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_ATTESTATION_VERIFICATION_KEY_UTILS_H_
#define COMPONENTS_PRIVATE_AI_ATTESTATION_VERIFICATION_KEY_UTILS_H_

#include <map>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "components/private_ai/attestation/server_verification_key.h"
#include "crypto/signature_verifier.h"

namespace private_ai {

struct VerificationKey {
  VerificationKey(std::vector<uint8_t> public_key,
                  crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                  OutputPrefixType output_prefix_type);
  ~VerificationKey();

  VerificationKey(VerificationKey&&);
  VerificationKey& operator=(VerificationKey&&);

  std::vector<uint8_t> public_key;  // SPKI format
  crypto::SignatureVerifier::SignatureAlgorithm algorithm;
  OutputPrefixType output_prefix_type;
};

// Loads all supported verification keys from a serialized Tink keyset into a
// map, keyed by their key ID.
std::map<uint32_t, VerificationKey> LoadVerificationKeys(
    base::span<const ProcessedKey> processed_keys);

// Parses a Tink-formatted signature to extract the key ID and the raw
// signature.
std::optional<std::pair<uint32_t, base::span<const uint8_t>>>
ParseTinkSignature(base::span<const uint8_t> signature);

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_ATTESTATION_VERIFICATION_KEY_UTILS_H_
