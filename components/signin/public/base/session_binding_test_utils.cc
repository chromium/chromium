// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/session_binding_test_utils.h"

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace signin {

namespace {
absl::optional<std::vector<uint8_t>> ConvertRawSignatureToDER(
    base::span<const uint8_t> raw_signature) {
  const size_t kMaxBytesPerBN = 32;
  if (raw_signature.size() != 2 * kMaxBytesPerBN) {
    return absl::nullopt;
  }
  base::span<const uint8_t> r_bytes = raw_signature.first(kMaxBytesPerBN);
  base::span<const uint8_t> s_bytes = raw_signature.subspan(kMaxBytesPerBN);

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig || !BN_bin2bn(r_bytes.data(), r_bytes.size(), ecdsa_sig->r) ||
      !BN_bin2bn(s_bytes.data(), s_bytes.size(), ecdsa_sig->s)) {
    return absl::nullopt;
  }

  uint8_t* signature_bytes;
  size_t signature_len;
  if (!ECDSA_SIG_to_bytes(&signature_bytes, &signature_len, ecdsa_sig.get())) {
    return absl::nullopt;
  }
  // Frees memory allocated by `ECDSA_SIG_to_bytes()`.
  bssl::UniquePtr<uint8_t> delete_signature(signature_bytes);
  return std::vector<uint8_t>(signature_bytes, signature_bytes + signature_len);
}
}  // namespace

bool VerifyJwtSignature(base::StringPiece jwt,
                        crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                        base::span<const uint8_t> public_key) {
  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 3U) {
    return false;
  }
  std::string signature_str;
  if (!base::Base64UrlDecode(parts[2],
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &signature_str)) {
    return false;
  }
  std::vector<uint8_t> signature(signature_str.begin(), signature_str.end());
  if (algorithm == crypto::SignatureVerifier::ECDSA_SHA256) {
    absl::optional<std::vector<uint8_t>> der_signature =
        ConvertRawSignatureToDER(base::as_bytes(base::make_span(signature)));
    if (!der_signature) {
      return false;
    }
    signature = std::move(der_signature).value();
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(algorithm, signature, public_key)) {
    return false;
  }
  verifier.VerifyUpdate(
      base::as_bytes(base::make_span(base::StrCat({parts[0], ".", parts[1]}))));
  return verifier.VerifyFinal();
}

}  // namespace signin
