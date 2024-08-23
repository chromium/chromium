// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"

#include "base/ranges/algorithm.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package::test {

Ed25519KeyPair Ed25519KeyPair::CreateRandom(bool produce_invalid_signature) {
  std::array<uint8_t, ED25519_PUBLIC_KEY_LEN> public_key;
  std::array<uint8_t, ED25519_PRIVATE_KEY_LEN> private_key;
  ED25519_keypair(public_key.data(), private_key.data());
  return Ed25519KeyPair(std::move(public_key), std::move(private_key),
                        produce_invalid_signature);
}

Ed25519KeyPair::Ed25519KeyPair(
    base::span<const uint8_t, ED25519_PUBLIC_KEY_LEN> public_key_bytes,
    base::span<const uint8_t, ED25519_PRIVATE_KEY_LEN> private_key_bytes,
    bool produce_invalid_signature)
    : public_key(Ed25519PublicKey::Create(public_key_bytes)),
      produce_invalid_signature(produce_invalid_signature) {
  std::array<uint8_t, ED25519_PRIVATE_KEY_LEN> private_key_array;
  base::ranges::copy(private_key_bytes, private_key_array.begin());
  private_key = std::move(private_key_array);
}

Ed25519KeyPair::Ed25519KeyPair(const Ed25519KeyPair&) = default;
Ed25519KeyPair& Ed25519KeyPair::operator=(const Ed25519KeyPair&) = default;

Ed25519KeyPair::Ed25519KeyPair(Ed25519KeyPair&&) noexcept = default;
Ed25519KeyPair& Ed25519KeyPair::operator=(Ed25519KeyPair&&) noexcept = default;

Ed25519KeyPair::~Ed25519KeyPair() = default;

std::vector<uint8_t> SignMessage(base::span<const uint8_t> message,
                                 const Ed25519KeyPair& key_pair) {
  std::vector<uint8_t> signature(ED25519_SIGNATURE_LEN);
  CHECK_EQ(key_pair.private_key.size(),
           static_cast<size_t>(ED25519_PRIVATE_KEY_LEN));
  CHECK_EQ(ED25519_sign(signature.data(), message.data(), message.size(),
                        key_pair.private_key.data()),
           1);
  if (key_pair.produce_invalid_signature) {
    signature[0] ^= 0xff;
  }
  return signature;
}

}  // namespace web_package::test
