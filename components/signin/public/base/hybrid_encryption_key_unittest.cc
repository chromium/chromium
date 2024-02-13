// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/tink_key.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

class HybridEncryptionKeyTest : public testing::Test {
 public:
  std::vector<uint8_t> GetPublicKey(HybridEncryptionKey& key) {
    return key.GetPublicKey();
  }

  HybridEncryptionKey CreateTestHybridEncryptionKey() {
    static const uint8_t kPrivateKey[X25519_PRIVATE_KEY_LEN] = {
        0xbc, 0xb5, 0x51, 0x29, 0x31, 0x10, 0x30, 0xc9, 0xed, 0x26, 0xde,
        0xd4, 0xb3, 0xdf, 0x3a, 0xce, 0x06, 0x8a, 0xee, 0x17, 0xab, 0xce,
        0xd7, 0xdb, 0xf3, 0x11, 0xe5, 0xa8, 0xf3, 0xb1, 0x8e, 0x24};
    return HybridEncryptionKey(kPrivateKey);
  }
};

TEST_F(HybridEncryptionKeyTest, ConstructingNewKeyMakesDifferentKey) {
  HybridEncryptionKey key1 = HybridEncryptionKey();
  HybridEncryptionKey key2 = HybridEncryptionKey();
  EXPECT_NE(GetPublicKey(key1), GetPublicKey(key2));
}

TEST_F(HybridEncryptionKeyTest, EncryptAndDecrypt) {
  HybridEncryptionKey key = CreateTestHybridEncryptionKey();
  std::string plaintext = "test";
  std::vector<uint8_t> ciphertext =
      key.EncryptForTesting(base::as_bytes(base::make_span(plaintext)));
  std::optional<std::vector<uint8_t>> maybe_plaintext = key.Decrypt(ciphertext);
  ASSERT_TRUE(maybe_plaintext.has_value());
  std::string decrypted_plaintext(maybe_plaintext->begin(),
                                  maybe_plaintext->end());
  EXPECT_EQ(decrypted_plaintext, plaintext);
}

TEST_F(HybridEncryptionKeyTest, EncryptAndDecryptEmpty) {
  HybridEncryptionKey key = CreateTestHybridEncryptionKey();
  std::string plaintext = "";
  std::vector<uint8_t> ciphertext =
      key.EncryptForTesting(base::as_bytes(base::make_span(plaintext)));
  std::optional<std::vector<uint8_t>> maybe_plaintext = key.Decrypt(ciphertext);
  ASSERT_TRUE(maybe_plaintext.has_value());
  std::string decrypted_plaintext(maybe_plaintext->begin(),
                                  maybe_plaintext->end());
  EXPECT_EQ(decrypted_plaintext, plaintext);
}

TEST_F(HybridEncryptionKeyTest, InvalidCiphertextTooShort) {
  HybridEncryptionKey key = CreateTestHybridEncryptionKey();
  std::vector<uint8_t> ciphertext = {0, 1, 2};
  std::optional<std::vector<uint8_t>> maybe_plaintext = key.Decrypt(ciphertext);
  EXPECT_FALSE(maybe_plaintext.has_value());
}

TEST_F(HybridEncryptionKeyTest, ExportPublicKeyStoresCorrectKeyBytes) {
  HybridEncryptionKey key = CreateTestHybridEncryptionKey();
  std::string exported_key_str = key.ExportPublicKey();
  tink::Keyset keyset;
  keyset.ParseFromString(exported_key_str);
  EXPECT_EQ(keyset.key_size(), 1);

  tink::HpkePublicKey hpke_key;
  hpke_key.ParseFromString(keyset.key(0).key_data().value());
  std::string public_key_str = hpke_key.public_key();
  std::vector<uint8_t> public_key(public_key_str.begin(), public_key_str.end());
  EXPECT_EQ(public_key, GetPublicKey(key));
}
