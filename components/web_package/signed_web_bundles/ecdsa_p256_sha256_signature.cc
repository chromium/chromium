// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"

#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_utils.h"

namespace web_package {

// static
base::expected<EcdsaP256SHA256Signature, std::string>
EcdsaP256SHA256Signature::Create(base::span<const uint8_t> bytes) {
  static constexpr size_t kMinLength = 64;
  static constexpr size_t kMaxLength = 72;
  if (bytes.size() < kMinLength || bytes.size() > kMaxLength) {
    return base::unexpected(base::StringPrintf(
        "The ECDSA P-256 SHA-256 signature does not have the correct length. "
        "Expected from %zu to %zu bytes, but received %zu bytes.",
        kMinLength, kMaxLength, bytes.size()));
  }
  return EcdsaP256SHA256Signature({bytes.begin(), bytes.end()});
}

EcdsaP256SHA256Signature::~EcdsaP256SHA256Signature() = default;

EcdsaP256SHA256Signature::EcdsaP256SHA256Signature(
    const EcdsaP256SHA256Signature&) = default;

EcdsaP256SHA256Signature::EcdsaP256SHA256Signature(EcdsaP256SHA256Signature&&) =
    default;

EcdsaP256SHA256Signature& EcdsaP256SHA256Signature::operator=(
    const EcdsaP256SHA256Signature&) = default;

EcdsaP256SHA256Signature& EcdsaP256SHA256Signature::operator=(
    EcdsaP256SHA256Signature&&) = default;

EcdsaP256SHA256Signature::EcdsaP256SHA256Signature(
    mojo::DefaultConstruct::Tag) {}

EcdsaP256SHA256Signature::EcdsaP256SHA256Signature(std::vector<uint8_t> bytes)
    : bytes_(std::move(bytes)) {}

[[nodiscard]] bool EcdsaP256SHA256Signature::Verify(
    base::span<const uint8_t> message,
    const EcdsaP256PublicKey& public_key) const {
  return internal::VerifyMessageSignedWithEcdsaP256SHA256(
      message, /*signature=*/bytes(), public_key);
}

}  // namespace web_package
