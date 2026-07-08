// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/crypto/agile_symmetric_key.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/model/crypto/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::Optional;

std::vector<uint8_t> HexStringToBytes(std::string_view hex) {
  std::vector<uint8_t> bytes;
  CHECK(base::HexStringToBytes(hex, &bytes));
  return bytes;
}

std::string HexStringToString(std::string_view hex) {
  std::string str;
  CHECK(base::HexStringToString(hex, &str));
  return str;
}

TEST(AgileSymmetricKeyTest, FromProtoShouldRejectIncorrectKeySizes) {
  const std::vector<uint8_t> kShortKey256(31, 1);
  const std::vector<uint8_t> kLongKey256(33, 1);

  sync_pb::AgileSymmetricKey proto;

  // AES-256 GCM.
  proto.mutable_aes_256_gcm()->set_key(kShortKey256.data(),
                                       kShortKey256.size());
  EXPECT_EQ(AgileSymmetricKey::FromProto(proto), nullptr);

  proto.clear_key_type();
  proto.mutable_aes_256_gcm()->set_key(kLongKey256.data(), kLongKey256.size());
  EXPECT_EQ(AgileSymmetricKey::FromProto(proto), nullptr);
}

TEST(AgileSymmetricKeyTest, FromProtoShouldAcceptKeysOfExactSize) {
  const std::vector<uint8_t> kValidKey256(32, 1);

  sync_pb::AgileSymmetricKey proto;
  proto.mutable_aes_256_gcm()->set_key(kValidKey256.data(),
                                       kValidKey256.size());
  EXPECT_NE(AgileSymmetricKey::FromProto(proto), nullptr);
}

TEST(AgileSymmetricKeyTest, ShouldCreateRandom) {
  const std::unique_ptr<AgileSymmetricKey> key1 =
      AgileSymmetricKey::CreateRandom();
  ASSERT_NE(key1, nullptr);
  EXPECT_TRUE(key1->ToProto().has_aes_256_gcm());
  EXPECT_EQ(key1->ToProto().aes_256_gcm().key().size(), 32u);

  const std::unique_ptr<AgileSymmetricKey> key2 =
      AgileSymmetricKey::CreateRandom();
  ASSERT_NE(key2, nullptr);
  EXPECT_NE(key1->ToProto().aes_256_gcm().key(),
            key2->ToProto().aes_256_gcm().key());
}

TEST(AgileSymmetricKeyTest, ShouldEncryptAndDecrypt) {
  const std::unique_ptr<AgileSymmetricKey> nigori =
      AgileSymmetricKey::CreateRandom();
  ASSERT_NE(nigori, nullptr);

  const std::vector<uint8_t> plaintext = {1, 2, 3, 4};
  const std::vector<uint8_t> encrypted = nigori->Encrypt(plaintext);
  // Encrypted should not be plaintext.
  EXPECT_NE(plaintext, encrypted);

  EXPECT_THAT(nigori->Decrypt(encrypted), Optional(plaintext));
}

TEST(AgileSymmetricKeyTest, ShouldFailToDecryptWithWrongKey) {
  const std::unique_ptr<AgileSymmetricKey> nigori1 =
      AgileSymmetricKey::CreateRandom();
  const std::unique_ptr<AgileSymmetricKey> nigori2 =
      AgileSymmetricKey::CreateRandom();

  ASSERT_NE(nigori1, nullptr);
  ASSERT_NE(nigori2, nullptr);

  const std::vector<uint8_t> plaintext = {1, 2, 3, 4};
  const std::vector<uint8_t> encrypted = nigori1->Encrypt(plaintext);
  EXPECT_EQ(nigori2->Decrypt(encrypted), std::nullopt);
}

TEST(AgileSymmetricKeyTest, ShouldFailToDecryptCorruptedData) {
  const std::unique_ptr<AgileSymmetricKey> nigori =
      AgileSymmetricKey::CreateRandom();
  ASSERT_NE(nigori, nullptr);

  const std::vector<uint8_t> plaintext = {1, 2, 3, 4};
  const std::vector<uint8_t> encrypted = nigori->Encrypt(plaintext);

  // Truncated payload.
  std::vector<uint8_t> truncated = encrypted;
  truncated.pop_back();
  EXPECT_EQ(nigori->Decrypt(truncated), std::nullopt);

  // Modified ciphertext/tag.
  std::vector<uint8_t> modified_tag = encrypted;
  modified_tag.back() ^= 1;
  EXPECT_EQ(nigori->Decrypt(modified_tag), std::nullopt);

  // Corrupted nonce.
  std::vector<uint8_t> wrong_header = encrypted;
  wrong_header[0] ^= 1;
  EXPECT_EQ(nigori->Decrypt(wrong_header), std::nullopt);
}

TEST(AgileSymmetricKeyTest, ShouldRoundTripProtoAes256Gcm) {
  const std::unique_ptr<AgileSymmetricKey> original_key =
      AgileSymmetricKey::CreateRandom();
  ASSERT_NE(original_key, nullptr);

  const sync_pb::AgileSymmetricKey proto = original_key->ToProto();
  EXPECT_TRUE(proto.has_aes_256_gcm());
  EXPECT_EQ(proto.aes_256_gcm().key().size(), 32u);

  const std::unique_ptr<AgileSymmetricKey> restored_key =
      AgileSymmetricKey::FromProto(proto);
  ASSERT_NE(restored_key, nullptr);
  EXPECT_TRUE(restored_key->ToProto().has_aes_256_gcm());

  // Test that restored key works for decryption.
  const std::vector<uint8_t> plaintext = {1, 2, 3, 4};
  const std::vector<uint8_t> encrypted = original_key->Encrypt(plaintext);
  EXPECT_THAT(restored_key->Decrypt(encrypted), Optional(plaintext));
}

TEST(AgileSymmetricKeyTest, ShouldSupportChacha20Poly1305) {
  const std::vector<uint8_t> kKey(32, 1);

  sync_pb::AgileSymmetricKey proto;
  proto.mutable_chacha20_poly1305()->set_key(kKey.data(), kKey.size());

  const std::unique_ptr<AgileSymmetricKey> key =
      AgileSymmetricKey::FromProto(proto);
  ASSERT_NE(key, nullptr);
  EXPECT_TRUE(key->ToProto().has_chacha20_poly1305());

  const std::vector<uint8_t> plaintext = {1, 2, 3, 4};
  const std::vector<uint8_t> encrypted = key->Encrypt(plaintext);
  EXPECT_THAT(key->Decrypt(encrypted), Optional(plaintext));
}

TEST(AgileSymmetricKeyTest, ShouldSupportLegacyNigori) {
  const std::string kUserKey = "1234567890123456";
  const std::string kEncryptionKey = "abcdefghijklmnop";
  const std::string kMacKey = "qrstuvwxyz123456";

  sync_pb::NigoriKey nigori_key_proto;
  nigori_key_proto.set_deprecated_user_key(kUserKey);
  nigori_key_proto.set_encryption_key(kEncryptionKey);
  nigori_key_proto.set_mac_key(kMacKey);

  std::unique_ptr<AgileSymmetricKey> key =
      AgileSymmetricKey::FromLegacyNigoriProto(nigori_key_proto);
  ASSERT_NE(key, nullptr);

  // Test encryption/decryption.
  const std::vector<uint8_t> plaintext = {'h', 'e', 'l', 'l', 'o'};
  const std::vector<uint8_t> encrypted = key->Encrypt(plaintext);
  EXPECT_THAT(key->Decrypt(encrypted), Optional(plaintext));

  // Test bidirectional compatibility with raw Nigori class.
  std::unique_ptr<Nigori> test_nigori = Nigori::CreateByImport(
      NigoriPassKey::ForTesting(), kUserKey, kEncryptionKey, kMacKey);
  ASSERT_NE(test_nigori, nullptr);

  // Decrypt ciphertext generated by AgileSymmetricKey using raw Nigori.
  EXPECT_THAT(test_nigori->DecryptFromBytes(encrypted), Optional(plaintext));

  // Decrypt ciphertext generated by raw Nigori using AgileSymmetricKey.
  const std::vector<uint8_t> expected_decrypted = {'h', 'e', 'l', 'l', 'o', ' ',
                                                   'a', 'g', 'i', 'l', 'e'};
  const std::vector<uint8_t> raw_encrypted =
      test_nigori->EncryptToBytes(expected_decrypted);

  EXPECT_THAT(key->Decrypt(raw_encrypted), Optional(expected_decrypted));

  // Test proto roundtrip.
  sync_pb::AgileSymmetricKey proto = key->ToProto();
  EXPECT_TRUE(proto.has_legacy_nigori());
  EXPECT_EQ(proto.legacy_nigori().deprecated_user_key(), kUserKey);
  EXPECT_EQ(proto.legacy_nigori().encryption_key(), kEncryptionKey);
  EXPECT_EQ(proto.legacy_nigori().mac_key(), kMacKey);

  std::unique_ptr<AgileSymmetricKey> restored =
      AgileSymmetricKey::FromProto(proto);
  ASSERT_NE(restored, nullptr);
  EXPECT_THAT(restored->Decrypt(encrypted), Optional(plaintext));
}

TEST(AgileSymmetricKeyTest, ShouldDecryptGoldenAes256Gcm) {
  const std::vector<uint8_t> kKey(32, 1);
  const std::vector<uint8_t> kGoldenEncryptedBlob = HexStringToBytes(
      "7FA3DBA8B38EDBC499214CCB278464946F21D7EDB8F3D9779ECE07965B769AD98580A7"
      "259FE5624ECC");

  sync_pb::AgileSymmetricKey proto;
  proto.mutable_aes_256_gcm()->set_key(kKey.data(), kKey.size());
  auto key = AgileSymmetricKey::FromProto(proto);
  ASSERT_NE(key, nullptr);

  const std::vector<uint8_t> expected_plaintext = {
      'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
  EXPECT_THAT(key->Decrypt(kGoldenEncryptedBlob), Optional(expected_plaintext));
}

TEST(AgileSymmetricKeyTest, ShouldDecryptGoldenChacha20Poly1305) {
  const std::vector<uint8_t> kKey(32, 2);
  const std::vector<uint8_t> kGoldenEncryptedBlob = HexStringToBytes(
      "58F74C04BFE8C0FBD07956B5AB62E01F5885E523E7927E2BD245516FADA6AD82CC3FFE"
      "BA01685D5DA7");

  sync_pb::AgileSymmetricKey proto;
  proto.mutable_chacha20_poly1305()->set_key(kKey.data(), kKey.size());
  auto key = AgileSymmetricKey::FromProto(proto);
  ASSERT_NE(key, nullptr);

  const std::vector<uint8_t> expected_plaintext = {
      'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
  EXPECT_THAT(key->Decrypt(kGoldenEncryptedBlob), Optional(expected_plaintext));
}

TEST(AgileSymmetricKeyTest, ShouldDecryptGoldenLegacyNigori) {
  const std::string kUserKey =
      HexStringToString("025599e143c4923d77f65b99d97019a3");
  const std::string kEncryptionKey =
      HexStringToString("4596bf346572497d92b2a0e2146d93c1");
  const std::string kMacKey =
      HexStringToString("2292ad9db96fe590b22a58db50f6f545");

  const std::vector<uint8_t> kGoldenEncryptedBlob = HexStringToBytes(
      "03A49A4C4771DBCB974F55CAC38416492B3B643D2273F35424DA7FF80CCCE9AFB58C46"
      "F4975F1EE64A9C82840BB9F8A60CA6470A22ED57320F5F3E50297581C9");

  sync_pb::NigoriKey proto;
  proto.set_deprecated_user_key(kUserKey);
  proto.set_encryption_key(kEncryptionKey);
  proto.set_mac_key(kMacKey);
  auto key = AgileSymmetricKey::FromLegacyNigoriProto(proto);
  ASSERT_NE(key, nullptr);

  const std::vector<uint8_t> expected_plaintext = {
      'H', 'e', 'l', 'l', 'o', ',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
  EXPECT_THAT(key->Decrypt(kGoldenEncryptedBlob), Optional(expected_plaintext));
}

}  // namespace
}  // namespace syncer
