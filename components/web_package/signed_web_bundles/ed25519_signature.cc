// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ed25519_signature.h"

#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

// We don't use `ED25519_SIGNATURE_LEN` in the header file, because we want to
// avoid including large BoringSSL headers in header files.
static_assert(Ed25519Signature::kLength == ED25519_SIGNATURE_LEN);

// static
base::expected<Ed25519Signature, std::string> Ed25519Signature::Create(
    base::span<const uint8_t> bytes) {
  auto sized_bytes = bytes.to_fixed_extent<kLength>();
  if (!sized_bytes) {
    return base::unexpected(base::StringPrintf(
        "The signature has the wrong length. Expected %zu, but got %zu bytes.",
        kLength, bytes.size()));
  }

  return Create(*sized_bytes);
}

bool Ed25519Signature::operator==(const Ed25519Signature& other) const {
  return *bytes_ == *other.bytes_;
}

bool Ed25519Signature::operator!=(const Ed25519Signature& other) const {
  return !operator==(other);
}

// static
Ed25519Signature Ed25519Signature::Create(
    base::span<const uint8_t, kLength> bytes) {
  std::array<uint8_t, kLength> array;
  base::ranges::copy(bytes, array.begin());
  return Ed25519Signature(array);
}

Ed25519Signature::Ed25519Signature(std::array<uint8_t, kLength>& bytes)
    : bytes_(bytes) {}

[[nodiscard]] bool Ed25519Signature::Verify(
    base::span<const uint8_t> message,
    const Ed25519PublicKey& public_key) const {
  const std::array<uint8_t, ED25519_PUBLIC_KEY_LEN>& public_key_bytes =
      public_key.bytes();
  const std::array<uint8_t, ED25519_SIGNATURE_LEN>& signature_bytes = bytes();
  return ED25519_verify(message.data(), message.size(), signature_bytes.data(),
                        public_key_bytes.data());
}

}  // namespace web_package
