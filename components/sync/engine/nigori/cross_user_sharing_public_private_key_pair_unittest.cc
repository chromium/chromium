// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"

#include <algorithm>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace syncer {
namespace {

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     GenerateNewKeyPairShouldAlwaysSucceed) {
  CrossUserSharingPublicPrivateKeyPair key =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  EXPECT_THAT(key.GetRawPrivateKey(), testing::SizeIs(X25519_PRIVATE_KEY_LEN));
  EXPECT_THAT(key.GetRawPublicKey(), testing::SizeIs(X25519_PUBLIC_VALUE_LEN));
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     GenerateNewKeyPairShouldGenerateDifferentKeys) {
  CrossUserSharingPublicPrivateKeyPair key_1 =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  CrossUserSharingPublicPrivateKeyPair key_2 =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  EXPECT_NE(key_1.GetRawPrivateKey(), key_2.GetRawPrivateKey());
  EXPECT_NE(key_1.GetRawPublicKey(), key_2.GetRawPublicKey());
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     GenerateNewKeyPairShouldGenerateDifferentPublicPrivateParts) {
  CrossUserSharingPublicPrivateKeyPair key =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  EXPECT_NE(key.GetRawPrivateKey(), key.GetRawPublicKey());
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     GeneratedPublicKeyShouldMatchX25519Derivation) {
  CrossUserSharingPublicPrivateKeyPair key =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  const std::array<uint8_t, X25519_PRIVATE_KEY_LEN> raw_private_key =
      key.GetRawPrivateKey();
  const std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> raw_public_key =
      key.GetRawPublicKey();

  uint8_t expected_public_key[X25519_PUBLIC_VALUE_LEN];
  uint8_t expected_private_key_arr[X25519_PRIVATE_KEY_LEN];
  std::copy(raw_private_key.begin(), raw_private_key.end(),
            expected_private_key_arr);
  X25519_public_from_private(expected_public_key, expected_private_key_arr);

  EXPECT_THAT(raw_public_key, testing::ElementsAreArray(expected_public_key));
}

TEST(CrossUserSharingPublicPrivateKeyPairTest, CreateByImportShouldSucceed) {
  std::vector<uint8_t> private_key(X25519_PRIVATE_KEY_LEN, 0xDE);

  absl::optional<CrossUserSharingPublicPrivateKeyPair> key =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  ASSERT_TRUE(key.has_value());

  std::array<uint8_t, X25519_PRIVATE_KEY_LEN> raw_private_key =
      key->GetRawPrivateKey();

  EXPECT_THAT(private_key, testing::ElementsAreArray(raw_private_key));
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     CreateByImportShouldFailOnShorterKey) {
  std::vector<uint8_t> private_key(X25519_PRIVATE_KEY_LEN - 1, 0xDE);

  absl::optional<CrossUserSharingPublicPrivateKeyPair> key =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  EXPECT_FALSE(key.has_value());
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     CreateByImportShouldFailOnLongerKey) {
  std::vector<uint8_t> private_key(X25519_PRIVATE_KEY_LEN + 1, 0xDE);

  absl::optional<CrossUserSharingPublicPrivateKeyPair> key =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  EXPECT_FALSE(key.has_value());
}

}  // namespace
}  // namespace syncer
