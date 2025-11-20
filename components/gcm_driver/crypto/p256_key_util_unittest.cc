// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include "base/base64.h"
#include "base/strings/string_view_util.h"
#include "crypto/keypair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// Precomputed private/public key-pair. Keys are stored on disk, so previously
// created values must continue to be usable for computing shared secrets.
const char kBobPrivateKey[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgS8wRbDOWz0lKExvIVQiRKtPAP8"
    "dgHUHAw5gyOd5d4jKhRANCAARZb49Va5MD/KcWtc0oiWc2e8njBDtQzj0mzcOl1fDSt16Pvu6p"
    "fTU3MTWnImDNnkPxtXm58K7Uax8jFxA4TeXJ";
const char kBobPublicKey[] =
    "BFlvj1VrkwP8pxa1zSiJZzZ7yeMEO1DOPSbNw6XV8NK3Xo++7ql9NTcxNaciYM2eQ/G1ebnwrt"
    "RrHyMXEDhN5ck=";

const char kCarolPrivateKey[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgmqy/ighwCm+RBP4Kct3rzaFEJ"
    "CZhokknro3KYsriurChRANCAAScr5sTsqmlP8SqiI+8fzxVLr1pby2HyG5mC5J0WSpYVIpMNS"
    "C16k1qcxqOJ4fiv8Ya47FYw/MIS7X1kobK27mP";
const char kCarolPublicKey[] =
    "BJyvmxOyqaU/xKqIj7x/PFUuvWlvLYfIbmYLknRZKlhUikw1ILXqTWpzGo4nh+K/xhrjsVjD8"
    "whLtfWShsrbuY8=";

// The shared secret between Bob and Carol.
const char kBobCarolSharedSecret[] =
    "AUNmKkgLLVLf6j/VnA9Eg1CiPSPfQHGirQj79n4vOyw=";

struct Keypair {
  // Load a Keypair from provided base64-encoded private and public keys. The
  // private key is in PKCS#8 PrivateKeyInfo format, and the public key is an
  // X9.62 uncompressed point encoded as a big-endian integer.
  Keypair(std::string_view priv_b64, std::string_view pub_b64)
      : priv(*crypto::keypair::PrivateKey::FromPrivateKeyInfo(
            *base::Base64Decode(priv_b64))),
        pub(base::as_string_view(*base::Base64Decode(pub_b64))) {}

  // Generate a new random keypair.
  Keypair()
      : priv(crypto::keypair::PrivateKey::GenerateEcP256()),
        pub(base::as_string_view(priv.ToUncompressedX962Point())) {}

  crypto::keypair::PrivateKey priv;
  std::string pub;
};

// Given two keypairs key0 and key1, perform shared-secret computation in both
// directions and check that the resulting secrets are nonempty and equal. If
// |out_secret| is non-null, fills it in with the generated secret.
void ExpectSharedSecretsAreEqual(const Keypair& key0,
                                 const Keypair& key1,
                                 std::string* out_secret = nullptr) {
  std::string secret_01, secret_10;
  ASSERT_TRUE(ComputeSharedP256Secret(key0.priv, key1.pub, &secret_01));
  ASSERT_TRUE(ComputeSharedP256Secret(key1.priv, key0.pub, &secret_10));
  EXPECT_GT(secret_01.size(), 0u);
  EXPECT_EQ(secret_01, secret_10);

  if (out_secret) {
    out_secret->assign(secret_01);
  }
}

TEST(P256KeyUtilTest, SharedSecretCalculation) {
  Keypair bob, alice;
  ExpectSharedSecretsAreEqual(alice, bob);
}

TEST(P256KeyUtilTest, SharedSecretWithInvalidKey) {
  Keypair bob;

  // Empty and too short peer public values should be considered invalid.
  std::string unused_shared_secret;
  ASSERT_FALSE(ComputeSharedP256Secret(bob.priv, "", &unused_shared_secret));
  ASSERT_FALSE(ComputeSharedP256Secret(bob.priv, bob.pub.substr(1),
                                       &unused_shared_secret));
}

TEST(P256KeyUtilTest, SharedSecretWithPreExistingKey) {
  Keypair bob(kBobPrivateKey, kBobPublicKey);

  // First verify against a newly created, ephemeral key-pair.
  Keypair alice;
  ExpectSharedSecretsAreEqual(bob, alice);

  // Then verify against another stored key-pair and shared secret.
  Keypair carol(kCarolPrivateKey, kCarolPublicKey);
  std::string secret;
  ExpectSharedSecretsAreEqual(carol, bob, &secret);

  const std::string expected_secret(
      base::as_string_view(*base::Base64Decode(kBobCarolSharedSecret)));
  EXPECT_EQ(secret, expected_secret);
}

}  // namespace

}  // namespace gcm
