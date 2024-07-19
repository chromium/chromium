// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/hybrid_encryption_key.h"

#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#include "components/signin/public/base/tink_key.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

TEST(HybridEncryptionKeyTest, ConstructingNewKeyMakesDifferentKey) {
  HybridEncryptionKey key1 = HybridEncryptionKey();
  HybridEncryptionKey key2 = HybridEncryptionKey();
  EXPECT_NE(GetHybridEncryptionPublicKeyForTesting(key1),
            GetHybridEncryptionPublicKeyForTesting(key2));
}

TEST(HybridEncryptionKeyTest, EncryptAndDecrypt) {
  HybridEncryptionKey key = CreateHybridEncryptionKeyForTesting();
  std::string plaintext = "test";
  std::vector<uint8_t> ciphertext =
      key.EncryptForTesting(base::as_bytes(base::make_span(plaintext)));
  std::optional<std::vector<uint8_t>> maybe_plaintext = key.Decrypt(ciphertext);
  ASSERT_TRUE(maybe_plaintext.has_value());
  std::string decrypted_plaintext(maybe_plaintext->begin(),
                                  maybe_plaintext->end());
  EXPECT_EQ(decrypted_plaintext, plaintext);
}

TEST(HybridEncryptionKeyTest, EncryptAndDecryptEmpty) {
  HybridEncryptionKey key = CreateHybridEncryptionKeyForTesting();
  std::string plaintext = "";
  std::vector<uint8_t> ciphertext =
      key.EncryptForTesting(base::as_bytes(base::make_span(plaintext)));
  std::optional<std::vector<uint8_t>> maybe_plaintext = key.Decrypt(ciphertext);
  ASSERT_TRUE(maybe_plaintext.has_value());
  std::string decrypted_plaintext(maybe_plaintext->begin(),
                                  maybe_plaintext->end());
  EXPECT_EQ(decrypted_plaintext, plaintext);
}

TEST(HybridEncryptionKeyTest, InvalidCiphertextTooShort) {
  HybridEncryptionKey key = CreateHybridEncryptionKeyForTesting();
  std::vector<uint8_t> ciphertext = {0, 1, 2};
  std::optional<std::vector<uint8_t>> maybe_plaintext = key.Decrypt(ciphertext);
  EXPECT_FALSE(maybe_plaintext.has_value());
}

TEST(HybridEncryptionKeyTest, ExportPublicKeyStoresCorrectKeyBytes) {
  HybridEncryptionKey key = CreateHybridEncryptionKeyForTesting();
  std::string exported_key_str = key.ExportPublicKey();
  tink::Keyset keyset;
  keyset.ParseFromString(exported_key_str);
  EXPECT_EQ(keyset.key_size(), 1);

  tink::HpkePublicKey hpke_key;
  hpke_key.ParseFromString(keyset.key(0).key_data().value());
  std::string public_key_str = hpke_key.public_key();
  std::vector<uint8_t> public_key(public_key_str.begin(), public_key_str.end());
  EXPECT_EQ(public_key, GetHybridEncryptionPublicKeyForTesting(key));
}
