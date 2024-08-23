// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/web_package/test_support/signed_web_bundles/ecdsa_p256_key_pair.h"

#include "base/check_is_test.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace web_package::test {
namespace {
// Nonce for obtaining deterministic ECDSA P-256 SHA-256 signatures. Taken from
// third_party/boringssl/src/crypto/fipsmodule/ecdsa/ecdsa_sign_tests.txt.
constexpr std::string_view kEcdsaP256SHA256NonceForTestingOnly =
    "36f853b5c54b1ec61588c9c6137eb56e7a708f09c57513093e4ecf6d739900e5";
}  // namespace

// static
EcdsaP256KeyPair EcdsaP256KeyPair::CreateRandom(
    bool produce_invalid_signature) {
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new());
  CHECK(ec_key);
  EC_KEY_set_group(ec_key.get(), EC_group_p256());
  CHECK_EQ(EC_KEY_generate_key(ec_key.get()), 1);

  std::array<uint8_t, EcdsaP256PublicKey::kLength> public_key;
  size_t export_length =
      EC_POINT_point2oct(EC_group_p256(), EC_KEY_get0_public_key(ec_key.get()),
                         POINT_CONVERSION_COMPRESSED, public_key.data(),
                         public_key.size(), /*ctx=*/nullptr);
  CHECK_EQ(export_length, EcdsaP256PublicKey::kLength);

  std::array<uint8_t, 32> private_key;
  CHECK_EQ(32u, EC_KEY_priv2oct(ec_key.get(), private_key.data(),
                                private_key.size()));

  return EcdsaP256KeyPair(public_key, private_key, produce_invalid_signature);
}

EcdsaP256KeyPair::EcdsaP256KeyPair(
    base::span<const uint8_t, EcdsaP256PublicKey::kLength> public_key_bytes,
    base::span<const uint8_t, 32> private_key_bytes,
    bool produce_invalid_signature)
    : public_key(*EcdsaP256PublicKey::Create(public_key_bytes)),
      produce_invalid_signature(produce_invalid_signature) {
  base::ranges::copy(private_key_bytes, private_key.begin());
}

EcdsaP256KeyPair::EcdsaP256KeyPair(const EcdsaP256KeyPair&) = default;
EcdsaP256KeyPair& EcdsaP256KeyPair::operator=(const EcdsaP256KeyPair&) =
    default;

EcdsaP256KeyPair::EcdsaP256KeyPair(EcdsaP256KeyPair&&) noexcept = default;
EcdsaP256KeyPair& EcdsaP256KeyPair::operator=(EcdsaP256KeyPair&&) noexcept =
    default;

EcdsaP256KeyPair::~EcdsaP256KeyPair() = default;

std::vector<uint8_t> SignMessage(base::span<const uint8_t> message,
                                 const EcdsaP256KeyPair& key_pair) {
  std::vector<uint8_t> signature = [&] {
    bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new());
    CHECK(ec_key);
    EC_KEY_set_group(ec_key.get(), EC_group_p256());
    CHECK_EQ(EC_KEY_oct2priv(ec_key.get(), key_pair.private_key.data(),
                             key_pair.private_key.size()),
             1);
    std::array<uint8_t, crypto::kSHA256Length> digest =
        crypto::SHA256Hash(message);

    // ECDSA signing with a fixed nonce is considered unsafe and is only
    // suitable for test scenarios.
    CHECK_IS_TEST();

    std::array<uint8_t, kEcdsaP256SHA256NonceForTestingOnly.size() / 2> nonce;
    CHECK(base::HexStringToSpan(kEcdsaP256SHA256NonceForTestingOnly, nonce));

    bssl::UniquePtr<ECDSA_SIG> sig(
        ECDSA_sign_with_nonce_and_leak_private_key_for_testing(
            digest.data(), digest.size(), ec_key.get(), nonce.data(),
            nonce.size()));
    CHECK(sig);

    uint8_t* signature_bytes;
    size_t signature_size;
    CHECK_EQ(ECDSA_SIG_to_bytes(&signature_bytes, &signature_size, sig.get()),
             1);
    bssl::UniquePtr<uint8_t> signature_bytes_deleter(signature_bytes);

    return std::vector<uint8_t>(signature_bytes,
                                signature_bytes + signature_size);
  }();

  if (key_pair.produce_invalid_signature) {
    signature[0] ^= 0xff;
  }
  return signature;
}
}  // namespace web_package::test
