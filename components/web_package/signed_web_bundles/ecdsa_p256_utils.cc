// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ecdsa_p256_utils.h"

#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"

namespace web_package::internal {

namespace {

bssl::UniquePtr<EC_KEY> EcdsaP256PublicKeyFromBytes(
    base::span<const uint8_t, EcdsaP256PublicKey::kLength> bytes) {
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new());
  CHECK(ec_key);
  EC_KEY_set_group(ec_key.get(), EC_group_p256());

  if (!EC_KEY_oct2key(ec_key.get(), bytes.data(), bytes.size(),
                      /*ctx=*/nullptr)) {
    // `bytes` doesn't represent a valid NIST P-256 point.
    return nullptr;
  }
  return ec_key;
}

}  // namespace

bool IsValidEcdsaP256PublicKey(
    base::span<const uint8_t, EcdsaP256PublicKey::kLength> public_key) {
  return !!EcdsaP256PublicKeyFromBytes(public_key);
}

bool VerifyMessageSignedWithEcdsaP256SHA256(
    base::span<const uint8_t> message,
    base::span<const uint8_t> signature,
    const EcdsaP256PublicKey& public_key) {
  bssl::UniquePtr<EC_KEY> ecdsa_public_key =
      EcdsaP256PublicKeyFromBytes(public_key.bytes());
  CHECK(ecdsa_public_key);

  std::array<uint8_t, crypto::kSHA256Length> digest =
      crypto::SHA256Hash(message);
  return ECDSA_verify(0, digest.data(), digest.size(), signature.data(),
                      signature.size(), ecdsa_public_key.get()) == 1;
}

}  // namespace web_package::internal
