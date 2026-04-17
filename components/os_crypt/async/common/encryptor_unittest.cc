// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor.mojom.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <wincrypt.h>

#include "base/win/scoped_localalloc.h"
#endif  // BUILDFLAG(IS_WIN)

namespace os_crypt_async {

enum class TestType {
  // Test that all operations work with a single key loaded.
  kWithSingleKey,
  // Test that all operations work with multiple keys loaded, and the first key
  // loaded is the default encryption provider.
  kWithMultipleKeys,
  // Test that all operations work with multiple keys loaded, and the second key
  // loaded is the default encryption provider.
  kWithMultipleKeysBackwards,
};

const auto kTestCases = {TestType::kWithSingleKey, TestType::kWithMultipleKeys,
                         TestType::kWithMultipleKeysBackwards};

class EncryptorTest : public ::testing::Test {
 protected:
  // This constant is taken from os_crypt_win.cc.
  static const size_t kKeyLength = 256 / 8;

  static_assert(kKeyLength == Encryptor::Key::kAES256GCMKeySize,
                "Key lengths must be the same.");

  static const Encryptor GetEncryptor() { return Encryptor(); }

  static const Encryptor GetEncryptor(
      Encryptor::KeyRing keys,
      const std::string& provider_for_encryption) {
    return Encryptor(std::move(keys), provider_for_encryption,
                     provider_for_encryption);
  }

  static const Encryptor GetEncryptor(
      Encryptor::KeyRing keys,
      const std::string& provider_for_encryption,
      const std::string& provider_for_os_crypt_sync_compatible_encryption) {
    return Encryptor(std::move(keys), provider_for_encryption,
                     provider_for_os_crypt_sync_compatible_encryption);
  }

  static Encryptor::Key GenerateRandomAES256TestKey() {
    Encryptor::Key key(
        crypto::RandBytesAsVector(Encryptor::Key::kAES256GCMKeySize),
        mojom::Algorithm::kAES256GCM);
    return key;
  }

  static Encryptor::Key DeriveAES256TestKey(std::string_view seed) {
    auto key_data =
        crypto::HkdfSha256(seed, {}, {}, Encryptor::Key::kAES256GCMKeySize);
    Encryptor::Key key(base::as_byte_span(key_data),
                       mojom::Algorithm::kAES256GCM);
    return key;
  }

  static Encryptor::Key CloneKey(const Encryptor::Key& key) {
    return key.Clone();
  }
};

class EncryptorParamTest : public EncryptorTest,
                           public ::testing::WithParamInterface<TestType> {
 protected:
  const Encryptor GetTestEncryptor() {
    switch (GetParam()) {
      case TestType::kWithSingleKey: {
        Encryptor::KeyRing key_ring;
        key_ring.emplace("TEST", GenerateRandomAES256TestKey());
        return GetEncryptor(std::move(key_ring), "TEST");
      }

      case TestType::kWithMultipleKeys: {
        Encryptor::KeyRing key_ring;
        key_ring.emplace("BLAH", GenerateRandomAES256TestKey());
        key_ring.emplace("TEST", GenerateRandomAES256TestKey());
        return GetEncryptor(std::move(key_ring), "BLAH");
      }

      case TestType::kWithMultipleKeysBackwards: {
        Encryptor::KeyRing key_ring;
        key_ring.emplace("TEST", GenerateRandomAES256TestKey());
        key_ring.emplace("BLAH", GenerateRandomAES256TestKey());
        return GetEncryptor(std::move(key_ring), "BLAH");
      }
    }
  }
};

TEST_P(EncryptorParamTest, StringInterface) {
  const Encryptor encryptor = GetTestEncryptor();
  std::string plaintext = "secrets";
  std::string ciphertext;
  EXPECT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));
  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_P(EncryptorParamTest, SpanInterface) {
  const Encryptor encryptor = GetTestEncryptor();
  std::string plaintext = "secrets";

  auto ciphertext = encryptor.EncryptString(plaintext);
  ASSERT_TRUE(ciphertext);

  auto decrypted = encryptor.DecryptData(*ciphertext);

  ASSERT_TRUE(decrypted);

  EXPECT_EQ(plaintext, *decrypted);
}

TEST_P(EncryptorParamTest, EncryptStringDecryptSpan) {
  const Encryptor encryptor = GetTestEncryptor();

  std::string plaintext = "secrets";
  std::string ciphertext;
  EXPECT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));

  auto decrypted = encryptor.DecryptData(base::as_byte_span(ciphertext));

  ASSERT_TRUE(decrypted);

  EXPECT_EQ(plaintext.size(), decrypted->size());

  ASSERT_TRUE(
      std::equal(plaintext.cbegin(), plaintext.cend(), decrypted->cbegin()));
}

TEST_P(EncryptorParamTest, EncryptSpanDecryptString) {
  const Encryptor encryptor = GetTestEncryptor();

  std::string plaintext = "secrets";

  auto ciphertext = encryptor.EncryptString(plaintext);
  ASSERT_TRUE(ciphertext);

  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(
      std::string(ciphertext->begin(), ciphertext->end()), &decrypted));
  EXPECT_EQ(plaintext.size(), decrypted.size());

  EXPECT_TRUE(
      std::equal(plaintext.cbegin(), plaintext.cend(), decrypted.cbegin()));
}

TEST_P(EncryptorParamTest, EncryptDecryptString16) {
  const Encryptor encryptor = GetTestEncryptor();

  const std::u16string plaintext = u"secrets";
  std::string ciphertext;
  ASSERT_TRUE(encryptor.EncryptString16(plaintext, &ciphertext));

  std::u16string decrypted;
  EXPECT_TRUE(encryptor.DecryptString16(ciphertext, &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST_P(EncryptorParamTest, EncryptEmpty) {
  const Encryptor encryptor = GetTestEncryptor();

  auto ciphertext = encryptor.EncryptString(std::string());
  ASSERT_TRUE(ciphertext);
  Encryptor::DecryptFlags flags;
  auto decrypted = encryptor.DecryptData(*ciphertext, &flags);
  ASSERT_FALSE(flags.should_reencrypt);
  ASSERT_TRUE(decrypted);
  EXPECT_TRUE(decrypted->empty());
}

// In a behavior change on Windows, Decrypt/Encrypt of empty data results in a
// success and an empty buffer. This was already the behavior on non-Windows so
// this change makes it consistent.
TEST_P(EncryptorParamTest, DecryptEmpty) {
  const Encryptor encryptor = GetTestEncryptor();

  Encryptor::DecryptFlags flags;
  auto plaintext = encryptor.DecryptData({}, &flags);
  ASSERT_FALSE(flags.should_reencrypt);
  ASSERT_TRUE(plaintext);
  EXPECT_TRUE(plaintext->empty());
}

// Non-Windows platforms can decrypt random data fine.
#if BUILDFLAG(IS_WIN)
TEST_P(EncryptorParamTest, DecryptInvalid) {
  const Encryptor encryptor = GetTestEncryptor();

  {
    std::vector<uint8_t> invalid_cipher(100);
    for (size_t c = 0u; c < invalid_cipher.size(); c++) {
      invalid_cipher[c] = c;
    }

    Encryptor::DecryptFlags flags;
    auto plaintext = encryptor.DecryptData(invalid_cipher, &flags);
    ASSERT_FALSE(flags.should_reencrypt);
    ASSERT_FALSE(plaintext);
  }
  {
    std::string plaintext;
    ASSERT_FALSE(encryptor.DecryptString("a", &plaintext));
    ASSERT_TRUE(plaintext.empty());
  }
}
#endif  // BUILDFLAG(IS_WIN)

INSTANTIATE_TEST_SUITE_P(All,
                         EncryptorParamTest,
                         ::testing::ValuesIn(kTestCases),
                         [](const ::testing::TestParamInfo<TestType>& info) {
                           switch (info.param) {
                             case TestType::kWithSingleKey:
                               return "WithSingleKey";
                             case TestType::kWithMultipleKeys:
                               return "WithMultipleKeys";
                             case TestType::kWithMultipleKeysBackwards:
                               return "WithMultipleKeysBackwards";
                           }
                         });

// This test verifies various combinations of multiple keys in a keyring, to
// make sure they are all handled correctly.
TEST_F(EncryptorTest, MultipleKeys) {
  Encryptor::Key foo_key = GenerateRandomAES256TestKey();
  Encryptor::Key bar_key = GenerateRandomAES256TestKey();

  Encryptor::KeyRing key_ring_both;
  key_ring_both.emplace("FOO", CloneKey(foo_key));
  key_ring_both.emplace("BAR", CloneKey(bar_key));

  const Encryptor foo_encryptor = GetEncryptor(std::move(key_ring_both), "FOO");

  // Should encrypt with FOO key.
  auto ciphertext = foo_encryptor.EncryptString("secret");
  ASSERT_TRUE(ciphertext);

  // Look into the data and verify that it's used the FOO key by looking for the
  // header.
  std::string foo_data_header("FOO");
  EXPECT_TRUE(std::equal(foo_data_header.cbegin(), foo_data_header.cend(),
                         ciphertext->cbegin()));

  // Decrypt with just the FOO key should succeed.
  {
    Encryptor::KeyRing key_ring_foo;
    key_ring_foo.emplace("FOO", CloneKey(foo_key));
    const Encryptor encryptor = GetEncryptor(std::move(key_ring_foo), "FOO");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ("secret", *decrypted);
  }

  // Decrypt with just the BAR key should fail.
  {
    Encryptor::KeyRing key_ring_bar;
    key_ring_bar.emplace("BAR", CloneKey(bar_key));
    const Encryptor encryptor = GetEncryptor(std::move(key_ring_bar), "BAR");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    EXPECT_FALSE(decrypted);
  }

  // Verify that order of keys in the keyring does not matter.
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BAR", CloneKey(bar_key));
    key_ring.emplace("FOO", CloneKey(foo_key));
    const Encryptor encryptor = GetEncryptor(std::move(key_ring), "FOO");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ("secret", *decrypted);

    // Verify that order does not affect which key is chosen to use for
    // encryption: "FOO" should always be picked. Note: because
    // Algorithm::kAES256GCM uses a random nonce, the encrypted values
    // themselves will be different.
    auto ciphertext2 = encryptor.EncryptString("secret");
    ASSERT_TRUE(ciphertext2);
    // Look into the data and verify that it's used the FOO key by looking for
    // the header.
    EXPECT_TRUE(std::equal(foo_data_header.cbegin(), foo_data_header.cend(),
                           ciphertext->cbegin()));
  }

  // Verify that the encryption provider does not matter when decrypting, it
  // just needs the key.
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BAR", CloneKey(bar_key));
    key_ring.emplace("FOO", CloneKey(foo_key));
    const Encryptor encryptor = GetEncryptor(std::move(key_ring), "BAR");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ("secret", *decrypted);
  }

  // Verify that an empty Encryptor can't decrypt FOO.
  {
    const Encryptor encryptor = GetEncryptor();
    auto decrypted = encryptor.DecryptData(*ciphertext);
    EXPECT_FALSE(decrypted);
  }
}

TEST_F(EncryptorTest, ShortCiphertext) {
  Encryptor::KeyRing key_ring;
  key_ring.emplace("TEST", GenerateRandomAES256TestKey());
  const Encryptor encryptor = GetEncryptor(std::move(key_ring), "TEST");
  // Create some bad data for the decryptor. Use the "TEST" prefix to ensure it
  // gets passed to the AES256 decryptor.
  std::string bad_data = "TEST";
  // This is the nonce length for this algorithm.
  static const size_t kNonceLength = 12u;
  for (size_t i = 0; i < kNonceLength * 2; i++) {
    bad_data += "a";
    auto decrypted = encryptor.DecryptData(base::as_byte_span(bad_data));
    EXPECT_FALSE(decrypted);
  }
}

TEST_F(EncryptorTest, IsEncryptionAvailable) {
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST", GenerateRandomAES256TestKey());
    const Encryptor encryptor = GetEncryptor(std::move(key_ring), "TEST");
    EXPECT_TRUE(encryptor.IsEncryptionAvailable());
    EXPECT_TRUE(encryptor.IsDecryptionAvailable());
  }
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST", GenerateRandomAES256TestKey());
    const Encryptor encryptor = GetEncryptor(std::move(key_ring), "BLAH");
    EXPECT_FALSE(encryptor.IsEncryptionAvailable());
    // Decryption for data encrypted with TEST key is available, but encryption
    // is not available as there is no key BLAH.
    EXPECT_TRUE(encryptor.IsDecryptionAvailable());
  }
}

// Test that Clone respects the option to a key that is os_crypt sync
// compatible.
TEST_F(EncryptorTest, Clone) {
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BLAH", GenerateRandomAES256TestKey());
    key_ring.emplace("TEST", GenerateRandomAES256TestKey());
    auto encryptor = GetEncryptor(std::move(key_ring), "TEST", "BLAH");

    {
      auto cloned_encryptor = encryptor.Clone(Encryptor::Option::kNone);
      EXPECT_EQ(cloned_encryptor.provider_for_encryption_, "TEST");
      EXPECT_EQ(cloned_encryptor.keys_.size(), 2u);
    }

    {
      auto cloned_encryptor =
          encryptor.Clone(Encryptor::Option::kEncryptSyncCompat);
      EXPECT_EQ(cloned_encryptor.provider_for_encryption_, "BLAH");
      EXPECT_EQ(cloned_encryptor.keys_.size(), 2u);
    }
  }

  // Test when the only key provider is not OSCrypt compatible. In this case, if
  // kEncryptSyncCompat is requested, then encryption should fail.
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BLAH", GenerateRandomAES256TestKey());
    auto encryptor = GetEncryptor(std::move(key_ring), "BLAH", std::string());
    EXPECT_EQ(encryptor.provider_for_encryption_, "BLAH");

    {
      auto cloned_encryptor = encryptor.Clone(Encryptor::Option::kNone);
      EXPECT_EQ(cloned_encryptor.provider_for_encryption_, "BLAH");
    }

    {
      auto cloned_encryptor =
          encryptor.Clone(Encryptor::Option::kEncryptSyncCompat);
      EXPECT_TRUE(cloned_encryptor.provider_for_encryption_.empty());
    }
  }

  // Test empty keyring.
  {
    const auto empty_encryptor = GetEncryptor();
    EXPECT_TRUE(empty_encryptor.provider_for_encryption_.empty());
    {
      auto cloned_encryptor =
          empty_encryptor.Clone(Encryptor::Option::kEncryptSyncCompat);
      EXPECT_TRUE(cloned_encryptor.provider_for_encryption_.empty());
    }

    {
      auto cloned_encryptor =
          empty_encryptor.Clone(Encryptor::Option::kEncryptSyncCompat);
      EXPECT_TRUE(cloned_encryptor.provider_for_encryption_.empty());
    }
  }
}

TEST_F(EncryptorTest, DecryptFlags) {
  std::string ciphertext;
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST", DeriveAES256TestKey("TEST"));
    const auto encryptor = GetEncryptor(std::move(key_ring), "TEST");
    ASSERT_TRUE(encryptor.EncryptString("secrets", &ciphertext));
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_FALSE(flags.should_reencrypt);
  }

  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BLAH", DeriveAES256TestKey("BLAH"));
    key_ring.emplace("TEST", DeriveAES256TestKey("TEST"));
    const auto encryptor = GetEncryptor(std::move(key_ring), "BLAH");
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_TRUE(flags.should_reencrypt);
  }
}

TEST_F(EncryptorTest, KeyAvailability) {
  std::string ciphertext;
  {
    // Encrypt some data using the TEST key.
    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST", DeriveAES256TestKey("TEST"));
    const auto encryptor = GetEncryptor(std::move(key_ring), "TEST");
    ASSERT_TRUE(encryptor.EncryptString("secrets", &ciphertext));
  }

  {
    // Load a key with the name TEST but it's not the same as before, so the
    // decrypt should fail permanently. This could happen e.g. if a key provider
    // decides it can never recover a key and generates a new one.
    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST", DeriveAES256TestKey("NOTTEST"));
    const auto encryptor = GetEncryptor(std::move(key_ring), "TEST");
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_FALSE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_FALSE(flags.temporarily_unavailable);
  }

  {
    // If the TEST key is not even there, it's also a permanent failure, since
    // key providers should signal a temporary failure using the proper API.
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BLAH", DeriveAES256TestKey("BLAH"));
    const auto encryptor = GetEncryptor(std::move(key_ring), "BLAH");
    Encryptor::DecryptFlags flags;
    std::string plaintext;
    ASSERT_FALSE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
    EXPECT_FALSE(flags.temporarily_unavailable);
  }
}

class EncryptorTraitsTest : public EncryptorTest {};

TEST_F(EncryptorTraitsTest, TraitsRoundTrip) {
  {
    const auto test_key1 =
        crypto::RandBytesAsVector(Encryptor::Key::kAES256GCMKeySize);
    const auto test_key2 =
        crypto::RandBytesAsVector(Encryptor::Key::kAES256GCMKeySize);

    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST1",
                     Encryptor::Key(test_key1, mojom::Algorithm::kAES256GCM));
    key_ring.emplace("TEST2",
                     Encryptor::Key(test_key2, mojom::Algorithm::kAES256GCM));

    Encryptor encryptor = GetEncryptor(std::move(key_ring), "TEST1");
    const auto ciphertext = encryptor.EncryptString("plaintext");
    ASSERT_TRUE(ciphertext.has_value());

    Encryptor roundtripped;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Encryptor>(
        encryptor, roundtripped));

    EXPECT_EQ(roundtripped.provider_for_encryption_, "TEST1");
    EXPECT_EQ(roundtripped.keys_.size(), 2U);

    EXPECT_EQ(roundtripped.keys_.at("TEST1"),
              Encryptor::Key(test_key1, mojom::Algorithm::kAES256GCM));
    EXPECT_EQ(roundtripped.keys_.at("TEST2"),
              Encryptor::Key(test_key2, mojom::Algorithm::kAES256GCM));
    const auto plaintext = roundtripped.DecryptData(*ciphertext);
    EXPECT_TRUE(plaintext.has_value());
    EXPECT_EQ(*plaintext, "plaintext");
  }

  {
    Encryptor encryptor = GetEncryptor();
    Encryptor roundtripped;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Encryptor>(
        encryptor, roundtripped));
    EXPECT_TRUE(roundtripped.keys_.empty());
    EXPECT_TRUE(roundtripped.provider_for_encryption_.empty());
  }

  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST", GenerateRandomAES256TestKey());

    Encryptor encryptor = GetEncryptor(std::move(key_ring), "TEST");

    // Reach into the encryptor and change the key length to an invalid length
    // for the kAES256GCM algorithm.
    encryptor.keys_.at("TEST")->key_.resize(8u);
    Encryptor roundtripped;

    // Mojo will fail gracefully to serialize this bad Encryptor.
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::Encryptor>(
        encryptor, roundtripped));
  }
}



}  // namespace os_crypt_async
