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

  std::optional<CrossUserSharingPublicPrivateKeyPair> key =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  ASSERT_TRUE(key.has_value());

  std::array<uint8_t, X25519_PRIVATE_KEY_LEN> raw_private_key =
      key->GetRawPrivateKey();

  EXPECT_THAT(private_key, testing::ElementsAreArray(raw_private_key));
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     CreateByImportShouldFailOnShorterKey) {
  std::vector<uint8_t> private_key(X25519_PRIVATE_KEY_LEN - 1, 0xDE);

  std::optional<CrossUserSharingPublicPrivateKeyPair> key =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  EXPECT_FALSE(key.has_value());
}

TEST(CrossUserSharingPublicPrivateKeyPairTest,
     CreateByImportShouldFailOnLongerKey) {
  std::vector<uint8_t> private_key(X25519_PRIVATE_KEY_LEN + 1, 0xDE);

  std::optional<CrossUserSharingPublicPrivateKeyPair> key =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  EXPECT_FALSE(key.has_value());
}

TEST(CrossUserSharingPublicPrivateKeyPairTest, ShouldEncryptAndDecrypt) {
  CrossUserSharingPublicPrivateKeyPair sender_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  CrossUserSharingPublicPrivateKeyPair recipient_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  const std::string plaintext = "Sharing is caring";

  std::optional<std::vector<uint8_t>> encrypted_message =
      sender_key_pair.HpkeAuthEncrypt(
          base::as_bytes(base::make_span(plaintext)),
          recipient_key_pair.GetRawPublicKey(), {});

  EXPECT_TRUE(encrypted_message.has_value());

  std::optional<std::vector<uint8_t>> decrypted_message =
      recipient_key_pair.HpkeAuthDecrypt(encrypted_message.value(),
                                         sender_key_pair.GetRawPublicKey(), {});

  EXPECT_TRUE(decrypted_message.has_value());
  EXPECT_THAT(decrypted_message.value(), testing::ElementsAreArray(plaintext));
}

// Ciphertext is too short to split into enc|ciphertext.
TEST(CrossUserSharingPublicPrivateKeyPairTest,
     ShouldReturnEmptyOnDecryptingShortCipherText) {
  CrossUserSharingPublicPrivateKeyPair sender_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  CrossUserSharingPublicPrivateKeyPair recipient_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  std::vector<uint8_t> encrypted_message = {0, 1, 2, 3};

  std::optional<std::vector<uint8_t>> decrypted_message =
      recipient_key_pair.HpkeAuthDecrypt(encrypted_message,
                                         sender_key_pair.GetRawPublicKey(), {});

  EXPECT_FALSE(decrypted_message.has_value());
}

// Encrypt for bad peer key (low-order X25519 points).
TEST(CrossUserSharingPublicPrivateKeyPairTest,
     ShouldReturnEmptyOnEncryptingForBadPeerKey) {
  CrossUserSharingPublicPrivateKeyPair sender_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  const std::vector<uint8_t> recipient_public_key(X25519_PUBLIC_VALUE_LEN,
                                                  0x00);

  std::optional<std::vector<uint8_t>> encrypted_message =
      sender_key_pair.HpkeAuthEncrypt(
          base::as_bytes(base::make_span("Sharing is caring")),
          recipient_public_key, {});

  EXPECT_FALSE(encrypted_message.has_value());
}

// Decrypt for bad peer key (low-order X25519 points).
TEST(CrossUserSharingPublicPrivateKeyPairTest,
     ShouldReturnEmptyOnDecryptingForBadPeerKey) {
  CrossUserSharingPublicPrivateKeyPair sender_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  CrossUserSharingPublicPrivateKeyPair recipient_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  const std::vector<uint8_t> sender_public_key(X25519_PUBLIC_VALUE_LEN, 0xDE);

  std::optional<std::vector<uint8_t>> encrypted_message =
      sender_key_pair.HpkeAuthEncrypt(
          base::as_bytes(base::make_span("Sharing is caring")),
          recipient_key_pair.GetRawPublicKey(), {});

  ASSERT_TRUE(encrypted_message.has_value());

  std::optional<std::vector<uint8_t>> decrypted_message =
      recipient_key_pair.HpkeAuthDecrypt(encrypted_message.value(),
                                         sender_public_key, {});

  EXPECT_FALSE(decrypted_message.has_value());
}

// Decrypt corrupted ciphertext.
TEST(CrossUserSharingPublicPrivateKeyPairTest,
     ShouldReturnEmptyOnDecryptingCorruptedCipherText) {
  CrossUserSharingPublicPrivateKeyPair sender_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  CrossUserSharingPublicPrivateKeyPair recipient_key_pair =
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();

  std::optional<std::vector<uint8_t>> encrypted_message =
      sender_key_pair.HpkeAuthEncrypt(
          base::as_bytes(base::make_span("Sharing is caring")),
          recipient_key_pair.GetRawPublicKey(), {});

  ASSERT_TRUE(encrypted_message.has_value());

  encrypted_message.value()[5] = encrypted_message.value()[5] ^ 0xDE;

  std::optional<std::vector<uint8_t>> decrypted_message =
      recipient_key_pair.HpkeAuthDecrypt(encrypted_message.value(),
                                         sender_key_pair.GetRawPublicKey(), {});

  EXPECT_FALSE(decrypted_message.has_value());
}

}  // namespace
}  // namespace syncer
