// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/session_binding_test_utils.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

constexpr size_t kJwtPartsCount = 3U;

// JWT parts listed in the order they appear in JWT.
enum class JwtPart : size_t { kHeader = 0, kPayload = 1, kSignature = 2 };

std::optional<std::string> ExtractJwtPart(std::string_view jwt, JwtPart part) {
  std::vector<std::string_view> encoded_parts = base::SplitStringPiece(
      jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (encoded_parts.size() != kJwtPartsCount) {
    return std::nullopt;
  }

  size_t part_index = static_cast<size_t>(part);
  CHECK_LT(part_index, kJwtPartsCount);
  std::string decoded_part;
  if (!base::Base64UrlDecode(encoded_parts[part_index],
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_part)) {
    return std::nullopt;
  }

  return decoded_part;
}

}  // namespace

testing::AssertionResult VerifyJwtSignature(
    std::string_view jwt,
    crypto::sign::SignatureKind algorithm,
    base::span<const uint8_t> public_key) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != kJwtPartsCount) {
    return testing::AssertionFailure()
           << "JWT contains " << parts.size() << " parts instead of "
           << kJwtPartsCount;
  }
  std::string signature_str;
  if (!base::Base64UrlDecode(parts[static_cast<size_t>(JwtPart::kSignature)],
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &signature_str)) {
    return testing::AssertionFailure()
           << "Failed to decode signature: " << signature_str;
  }
  std::vector<uint8_t> signature(signature_str.begin(), signature_str.end());

  std::optional<crypto::keypair::PublicKey> key =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(public_key);
  if (!key) {
    return testing::AssertionFailure() << "Failed to parse public key SPKI";
  }

  using enum crypto::sign::SignatureKind;
  switch (algorithm) {
    case RSA_PKCS1_SHA1:
    case RSA_PKCS1_SHA256:
    case RSA_PKCS1_SHA384:
    case RSA_PKCS1_SHA512:
    case RSA_PSS_SHA256:
    case RSA_PSS_SHA384:
    case RSA_PSS_SHA512:
      if (!key->IsRsa()) {
        return testing::AssertionFailure()
               << "Failed to verify signature: RSA key expected";
      }
      break;
    case ECDSA_SHA1:
    case ECDSA_SHA256:
    case ECDSA_SHA384:
    case ECDSA_SHA512: {
      if (!key->IsEc()) {
        return testing::AssertionFailure()
               << "Failed to verify signature: EC key expected";
      }
      std::optional<std::vector<uint8_t>> der_signature =
          crypto::ConvertEcdsaRawSignatureToDer(*key, signature);
      if (!der_signature) {
        return testing::AssertionFailure()
               << "Failed to convert raw signature to DER: " << signature_str;
      }
      signature = *std::move(der_signature);
      break;
    }
    case ED25519:
      if (!key->IsEd25519()) {
        return testing::AssertionFailure()
               << "Failed to verify signature: Ed25519 key expected";
      }
      break;
    case MLDSA_44:
      if (!key->IsMldsa44()) {
        return testing::AssertionFailure()
               << "Failed to verify signature: ML-DSA-44 key expected";
      }
      break;
    case MLDSA_65:
      if (!key->IsMldsa65()) {
        return testing::AssertionFailure()
               << "Failed to verify signature: ML-DSA-65 key expected";
      }
      break;
    case MLDSA_87:
      if (!key->IsMldsa87()) {
        return testing::AssertionFailure()
               << "Failed to verify signature: ML-DSA-87 key expected";
      }
      break;
  }

  std::string header_and_payload =
      base::StrCat({parts[static_cast<size_t>(JwtPart::kHeader)], ".",
                    parts[static_cast<size_t>(JwtPart::kPayload)]});
  return crypto::sign::Verify(algorithm, *key,
                              base::as_byte_span(header_and_payload), signature)
             ? testing::AssertionSuccess()
             : testing::AssertionFailure() << "Failed to verify signature";
}

std::optional<base::DictValue> ExtractHeaderFromJwt(std::string_view jwt) {
  std::optional<std::string> header = ExtractJwtPart(jwt, JwtPart::kHeader);
  if (!header) {
    return std::nullopt;
  }

  return base::JSONReader::ReadDict(*header,
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
}

std::optional<base::DictValue> ExtractPayloadFromJwt(std::string_view jwt) {
  std::optional<std::string> payload = ExtractJwtPart(jwt, JwtPart::kPayload);
  if (!payload) {
    return std::nullopt;
  }

  return base::JSONReader::ReadDict(*payload,
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
}

std::string EncryptValueWithEphemeralKey(
    const HybridEncryptionKey& ephemeral_key,
    std::string_view value) {
  std::optional<std::vector<uint8_t>> encrypted_value =
      ephemeral_key.EncryptForTesting(base::as_byte_span(value));
  if (!encrypted_value) {
    return std::string();
  }
  std::string base64_encrypted_value;
  base::Base64UrlEncode(*encrypted_value,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_encrypted_value);
  return base64_encrypted_value;
}

}  // namespace signin
