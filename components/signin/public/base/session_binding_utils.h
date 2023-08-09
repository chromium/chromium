// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class Time;
}

namespace signin {

// Creates header and payload parts of a registration JWT.
absl::optional<std::string>
CreateKeyRegistrationHeaderAndPayloadForTokenBinding(
    base::StringPiece client_id,
    base::StringPiece auth_code,
    const GURL& registration_url,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    base::Time timestamp);
absl::optional<std::string>
CreateKeyRegistrationHeaderAndPayloadForSessionBinding(
    base::StringPiece challenge,
    const GURL& registration_url,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    base::Time timestamp);

// Creates header and payload parts of an assertion JWT.
// TODO(b/279026351): Add support for "ephemeral_key".
absl::optional<std::string> CreateKeyAssertionHeaderAndPayload(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    base::StringPiece client_id,
    base::StringPiece challenge,
    const GURL& destination_url);

// Appends `signature` to provided `header_and_payload` to form a complete JWT.
std::string AppendSignatureToHeaderAndPayload(
    base::StringPiece header_and_payload,
    base::span<const uint8_t> signature);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_UTILS_H_
