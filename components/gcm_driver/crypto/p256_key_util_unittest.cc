// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include <stddef.h>

#include <set>

#include "base/base64.h"
#include "crypto/ec_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// A P-256 point in uncompressed form consists of 0x04 (to denote that the point
// is uncompressed per SEC1 2.3.3) followed by two, 32-byte field elements.
const size_t kUncompressedPointBytes = 1 + 2 * 32;

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

TEST(P256KeyUtilTest, UniqueKeyPairGeneration) {
  // Canary for determining that no key repetitions are found in few iterations.
  std::set<std::string> seen_private_keys;
  std::set<std::string> seen_public_keys;

  for (int iteration = 0; iteration < 10; ++iteration) {
    SCOPED_TRACE(iteration);

    std::string private_key, public_key;
    std::unique_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());
    ASSERT_TRUE(key);
    ASSERT_TRUE(GetRawPublicKey(*key, &public_key));
    ASSERT_TRUE(GetRawPrivateKey(*key, &private_key));

    EXPECT_NE(private_key, public_key);
    EXPECT_GT(private_key.size(), 0u);
    EXPECT_EQ(public_key.size(), kUncompressedPointBytes);

    EXPECT_EQ(0u, seen_private_keys.count(private_key));
    EXPECT_EQ(0u, seen_public_keys.count(public_key));

    seen_private_keys.insert(private_key);
    seen_public_keys.insert(public_key);
  }
}

TEST(P256KeyUtilTest, SharedSecretCalculation) {
  std::unique_ptr<crypto::ECPrivateKey> bob_key =
      crypto::ECPrivateKey::Create();
  std::unique_ptr<crypto::ECPrivateKey> alice_key =
      crypto::ECPrivateKey::Create();

  std::string alice_public_key, bob_public_key, alice_private_key,
      bob_private_key;
  ASSERT_TRUE(GetRawPublicKey(*bob_key, &bob_public_key));
  ASSERT_TRUE(GetRawPublicKey(*alice_key, &alice_public_key));
  ASSERT_TRUE(GetRawPrivateKey(*bob_key, &bob_private_key));
  ASSERT_TRUE(GetRawPrivateKey(*alice_key, &alice_private_key));
  ASSERT_NE(bob_public_key, alice_public_key);
  ASSERT_NE(bob_private_key, alice_private_key);

  std::string bob_shared_secret, alice_shared_secret;
  ASSERT_TRUE(
      ComputeSharedP256Secret(*bob_key, alice_public_key, &bob_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(*alice_key, bob_public_key,
                                      &alice_shared_secret));

  EXPECT_GT(bob_shared_secret.size(), 0u);
  EXPECT_EQ(bob_shared_secret, alice_shared_secret);

  std::string unused_shared_secret;

  // Empty and too short peer public values should be considered invalid.
  ASSERT_FALSE(ComputeSharedP256Secret(*bob_key, "", &unused_shared_secret));
  ASSERT_FALSE(ComputeSharedP256Secret(*bob_key, bob_public_key.substr(1),
                                       &unused_shared_secret));
}

TEST(P256KeyUtilTest, SharedSecretWithPreExistingKey) {
  std::string bob_private_key, bob_public_key;
  ASSERT_TRUE(base::Base64Decode(kBobPrivateKey, &bob_private_key));
  ASSERT_TRUE(base::Base64Decode(kBobPublicKey, &bob_public_key));

  std::vector<uint8_t> bob_private_key_vec(
    bob_private_key.begin(), bob_private_key.end());
  std::unique_ptr<crypto::ECPrivateKey> bob_key =
    crypto::ECPrivateKey::CreateFromPrivateKeyInfo(bob_private_key_vec);
  ASSERT_TRUE(bob_key);
  // First verify against a newly created, ephemeral key-pair.
  std::unique_ptr<crypto::ECPrivateKey> alice_key(
      crypto::ECPrivateKey::Create());
  std::string alice_public_key;
  ASSERT_TRUE(GetRawPublicKey(*alice_key, &alice_public_key));
  std::string bob_shared_secret, alice_shared_secret;

  ASSERT_TRUE(ComputeSharedP256Secret(*bob_key, alice_public_key,
                                      &bob_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(*alice_key, bob_public_key,
                                      &alice_shared_secret));

  EXPECT_GT(bob_shared_secret.size(), 0u);
  EXPECT_EQ(bob_shared_secret, alice_shared_secret);

  std::string carol_private_key, carol_public_key;
  ASSERT_TRUE(base::Base64Decode(kCarolPrivateKey, &carol_private_key));
  ASSERT_TRUE(base::Base64Decode(kCarolPublicKey, &carol_public_key));

  std::vector<uint8_t> carol_private_key_vec(
    carol_private_key.begin(), carol_private_key.end());
  std::unique_ptr<crypto::ECPrivateKey> carol_key =
    crypto::ECPrivateKey::CreateFromPrivateKeyInfo(carol_private_key_vec);
  ASSERT_TRUE(carol_key);
  bob_shared_secret.clear();
  std::string carol_shared_secret;

  // Then verify against another stored key-pair and shared secret.
  ASSERT_TRUE(ComputeSharedP256Secret(*bob_key, carol_public_key,
                                      &bob_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(*carol_key, bob_public_key,
                                      &carol_shared_secret));

  EXPECT_GT(carol_shared_secret.size(), 0u);
  EXPECT_EQ(carol_shared_secret, bob_shared_secret);

  std::string bob_carol_shared_secret;
  ASSERT_TRUE(base::Base64Decode(
      kBobCarolSharedSecret, &bob_carol_shared_secret));

  EXPECT_EQ(carol_shared_secret, bob_carol_shared_secret);
}

}  // namespace

}  // namespace gcm
