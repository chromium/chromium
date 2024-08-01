// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/signin/public/base/session_binding_test_utils.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace signin {

namespace {

constexpr size_t kJwtPartsCount = 3U;
constexpr uint8_t kJwtSeparatorArray[] = {'.'};

// JWT parts listed in the order they appear in JWT.
enum class JwtPart : size_t { kHeader = 0, kPayload = 1, kSignature = 2 };

std::optional<std::vector<uint8_t>> ConvertRawSignatureToDER(
    base::span<const uint8_t> raw_signature) {
  const size_t kMaxBytesPerBN = 32;
  if (raw_signature.size() != 2 * kMaxBytesPerBN) {
    return std::nullopt;
  }
  base::span<const uint8_t> r_bytes = raw_signature.first(kMaxBytesPerBN);
  base::span<const uint8_t> s_bytes = raw_signature.subspan(kMaxBytesPerBN);

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig || !BN_bin2bn(r_bytes.data(), r_bytes.size(), ecdsa_sig->r) ||
      !BN_bin2bn(s_bytes.data(), s_bytes.size(), ecdsa_sig->s)) {
    return std::nullopt;
  }

  uint8_t* signature_bytes;
  size_t signature_len;
  if (!ECDSA_SIG_to_bytes(&signature_bytes, &signature_len, ecdsa_sig.get())) {
    return std::nullopt;
  }
  // Frees memory allocated by `ECDSA_SIG_to_bytes()`.
  bssl::UniquePtr<uint8_t> delete_signature(signature_bytes);
  return std::vector<uint8_t>(signature_bytes, signature_bytes + signature_len);
}

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
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
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
  if (algorithm == crypto::SignatureVerifier::ECDSA_SHA256) {
    std::optional<std::vector<uint8_t>> der_signature =
        ConvertRawSignatureToDER(base::as_bytes(base::make_span(signature)));
    if (!der_signature) {
      return testing::AssertionFailure()
             << "Failed to convert raw signature to DER: " << signature_str;
    }
    signature = std::move(der_signature).value();
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(algorithm, signature, public_key)) {
    return testing::AssertionFailure()
           << "Failed to initialize the signature verifier";
  }
  verifier.VerifyUpdate(base::as_bytes(
      base::make_span(parts[static_cast<size_t>(JwtPart::kHeader)])));
  verifier.VerifyUpdate(kJwtSeparatorArray);
  verifier.VerifyUpdate(base::as_bytes(
      base::make_span(parts[static_cast<size_t>(JwtPart::kPayload)])));
  return verifier.VerifyFinal()
             ? testing::AssertionSuccess()
             : (testing::AssertionFailure() << "Invalid signature");
}

std::optional<base::Value::Dict> ExtractHeaderFromJwt(std::string_view jwt) {
  std::optional<std::string> header = ExtractJwtPart(jwt, JwtPart::kHeader);
  if (!header) {
    return std::nullopt;
  }

  return base::JSONReader::ReadDict(*header);
}

std::optional<base::Value::Dict> ExtractPayloadFromJwt(std::string_view jwt) {
  std::optional<std::string> payload = ExtractJwtPart(jwt, JwtPart::kPayload);
  if (!payload) {
    return std::nullopt;
  }

  return base::JSONReader::ReadDict(*payload);
}

}  // namespace signin
