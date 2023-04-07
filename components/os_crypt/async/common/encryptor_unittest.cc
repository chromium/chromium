// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor.mojom.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "crypto/random.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <wincrypt.h>

#include "base/win/scoped_localalloc.h"
#endif  // BUILDFLAG(IS_WIN)

namespace os_crypt_async {

namespace {

#if BUILDFLAG(IS_WIN)
// Utility function to encrypt data using the raw DPAPI interface.
bool EncryptStringWithDPAPI(const std::string& plaintext,
                            std::string& ciphertext) {
  DATA_BLOB input = {};
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  BOOL result = FALSE;
  DATA_BLOB output = {};
  result =
      ::CryptProtectData(&input, /*szDataDescr=*/L"",
                         /*pOptionalEntropy=*/nullptr, /*pvReserved=*/nullptr,
                         /*pPromptStruct=*/nullptr, /*dwFlags=*/0, &output);
  if (!result) {
    return false;
  }

  auto local_alloc = base::win::TakeLocalAlloc(output.pbData);

  static_assert(sizeof(std::string::value_type) == 1);

  ciphertext.assign(
      reinterpret_cast<std::string::value_type*>(local_alloc.get()),
      output.cbData);

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

// Helper function to verify that decryption using OSCrypt worked. This is
// platform dependent, as Windows will fail, but other platforms will return the
// ciphertext back.
[[nodiscard]] bool MaybeVerifyDecryptOperation(
    const absl::optional<std::string>& decrypted,
    base::span<const uint8_t> ciphertext) {
#if BUILDFLAG(IS_WIN)
  // On Windows, decryption fails, and decrypted will have no valid value.
  return !decrypted;
#else
  // On other platforms, OSCrypt does not recognise the data and it returns
  // the data without decrypting.
  if (!decrypted) {
    return false;
  }
  return decrypted == std::string(ciphertext.begin(), ciphertext.end());
#endif
}

}  // namespace

enum class TestType {
  // Test that all operations work with no keys.
  kEmptyPassThru,
  // Test that all operations work with a single key loaded.
  kWithSingleKey,
  // Test that all operations work with multiple keys loaded, and the first key
  // loaded is the default encryption provider.
  kWithMultipleKeys,
  // Test that all operations work with multiple keys loaded, and the second key
  // loaded is the default encryption provider.
  kWithMultipleKeysBackwards,
};

const auto kTestCases = {TestType::kEmptyPassThru, TestType::kWithSingleKey,
                         TestType::kWithMultipleKeys,
                         TestType::kWithMultipleKeysBackwards};

class EncryptorTestBase : public ::testing::Test {
 protected:
  // This constant is taken from os_crypt_win.cc.
  static const size_t kKeyLength = 256 / 8;

  static_assert(kKeyLength == Encryptor::Key::kAES256GCMKeySize,
                "Key lengths must be the same.");

  static const Encryptor GetEncryptor() { return Encryptor(); }

  static const Encryptor GetEncryptor(
      Encryptor::KeyRing keys,
      const std::string& provider_for_encryption) {
    return Encryptor(std::move(keys), provider_for_encryption);
  }

  static std::vector<uint8_t> GenerateRandomTestKey(size_t length) {
    std::vector<uint8_t> key_data(length);
    crypto::RandBytes(key_data);
    return key_data;
  }

  static Encryptor::Key GenerateRandomAES256TestKey() {
    return Encryptor::Key(
        GenerateRandomTestKey(Encryptor::Key::kAES256GCMKeySize),
        mojom::Algorithm::kAES256GCM);
  }
};

class EncryptorTestWithOSCrypt : public EncryptorTestBase {
 protected:
  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override {
    OSCryptMocker::TearDown();
#if BUILDFLAG(IS_WIN)
    OSCrypt::ResetStateForTesting();
#endif  // BUILDFLAG(IS_WIN)
  }
};

class EncryptorTest : public EncryptorTestWithOSCrypt,
                      public ::testing::WithParamInterface<TestType> {
 protected:
  const Encryptor GetTestEncryptor() {
    switch (GetParam()) {
      case TestType::kEmptyPassThru: {
        return GetEncryptor();
      }

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

TEST_P(EncryptorTest, StringInterface) {
  const Encryptor encryptor = GetTestEncryptor();
  std::string plaintext = "secrets";
  std::string ciphertext;
  EXPECT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));
  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_P(EncryptorTest, SpanInterface) {
  const Encryptor encryptor = GetTestEncryptor();
  std::string plaintext = "secrets";

  auto ciphertext = encryptor.EncryptString(plaintext);
  ASSERT_TRUE(ciphertext);

  auto decrypted = encryptor.DecryptData(*ciphertext);

  ASSERT_TRUE(decrypted);

  EXPECT_EQ(plaintext, *decrypted);
}

TEST_P(EncryptorTest, EncryptStringDecryptSpan) {
  const Encryptor encryptor = GetTestEncryptor();

  std::string plaintext = "secrets";
  std::string ciphertext;
  EXPECT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));

  auto decrypted =
      encryptor.DecryptData(base::as_bytes(base::make_span(ciphertext)));

  ASSERT_TRUE(decrypted);

  EXPECT_EQ(plaintext.size(), decrypted->size());

  ASSERT_TRUE(
      std::equal(plaintext.cbegin(), plaintext.cend(), decrypted->cbegin()));
}

TEST_P(EncryptorTest, EncryptSpanDecryptString) {
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

TEST_P(EncryptorTest, EncryptEmpty) {
  const Encryptor encryptor = GetTestEncryptor();

  auto ciphertext = encryptor.EncryptString(std::string());
  ASSERT_TRUE(ciphertext);

  auto decrypted = encryptor.DecryptData(*ciphertext);
  ASSERT_TRUE(decrypted);
  EXPECT_TRUE(decrypted->empty());
}

// In a behavior change on Windows, Decrypt/Encrypt of empty data results in a
// success and an empty buffer. This was already the behavior on non-Windows so
// this change makes it consistent.
TEST_P(EncryptorTest, DecryptEmpty) {
  const Encryptor encryptor = GetTestEncryptor();

  auto plaintext = encryptor.DecryptData({});
  ASSERT_TRUE(plaintext);
  EXPECT_TRUE(plaintext->empty());
}

// Non-Windows platforms can decrypt random data fine.
#if BUILDFLAG(IS_WIN)
TEST_P(EncryptorTest, DecryptInvalid) {
  const Encryptor encryptor = GetTestEncryptor();

  std::vector<uint8_t> invalid_cipher(100);
  for (size_t c = 0u; c < invalid_cipher.size(); c++) {
    invalid_cipher[c] = c;
  }

  auto plaintext = encryptor.DecryptData(invalid_cipher);
  ASSERT_FALSE(plaintext);
}
#endif  // BUILDFLAG(IS_WIN)

// Encryptor can decrypt data encrypted with OSCrypt.
TEST_P(EncryptorTest, DecryptFallback) {
  std::string ciphertext;
  EXPECT_TRUE(OSCrypt::EncryptString("secret", &ciphertext));

  const Encryptor encryptor = GetTestEncryptor();
  std::string decrypted;

  // Fallback to OSCrypt takes place.
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));

  EXPECT_EQ("secret", decrypted);
}

#if BUILDFLAG(IS_WIN)
// Encryptor should still decrypt data encrypted using DPAPI (pre-m79) by fall
// back to OSCrypt.
TEST_P(EncryptorTest, AncientFallback) {
  std::string ciphertext;
  EXPECT_TRUE(EncryptStringWithDPAPI("secret", ciphertext));

  std::string decrypted;
  const Encryptor encryptor = GetTestEncryptor();
  // Encryptor can still decrypt very old DPAPI data.
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));

  EXPECT_EQ("secret", decrypted);
}
#endif  // BUILDFLAG(IS_WIN)

INSTANTIATE_TEST_SUITE_P(All,
                         EncryptorTest,
                         ::testing::ValuesIn(kTestCases),
                         [](const ::testing::TestParamInfo<TestType>& info) {
                           switch (info.param) {
                             case TestType::kEmptyPassThru:
                               return "EmptyPassThru";
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
TEST_F(EncryptorTestBase, MultipleKeys) {
  Encryptor::Key foo_key = GenerateRandomAES256TestKey();
  Encryptor::Key bar_key = GenerateRandomAES256TestKey();

  Encryptor::KeyRing key_ring_both;
  key_ring_both.emplace("FOO", foo_key.Clone());
  key_ring_both.emplace("BAR", bar_key.Clone());

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
    key_ring_foo.emplace("FOO", foo_key.Clone());
    const Encryptor encryptor = GetEncryptor(std::move(key_ring_foo), "FOO");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ("secret", *decrypted);
  }

  // Decrypt with just the BAR key should fail.
  {
    Encryptor::KeyRing key_ring_bar;
    key_ring_bar.emplace("BAR", bar_key.Clone());
    const Encryptor encryptor = GetEncryptor(std::move(key_ring_bar), "BAR");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    EXPECT_TRUE(MaybeVerifyDecryptOperation(decrypted, *ciphertext));
  }

  // Verify that order of keys in the keyring does not matter.
  {
    Encryptor::KeyRing key_ring;
    key_ring.emplace("BAR", bar_key.Clone());
    key_ring.emplace("FOO", foo_key.Clone());
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
    key_ring.emplace("BAR", bar_key.Clone());
    key_ring.emplace("FOO", foo_key.Clone());
    const Encryptor encryptor = GetEncryptor(std::move(key_ring), "BAR");
    auto decrypted = encryptor.DecryptData(*ciphertext);
    ASSERT_TRUE(decrypted);
    EXPECT_EQ("secret", *decrypted);
  }

  // Verify that an empty Encryptor can't decrypt FOO.
  {
    const Encryptor encryptor = GetEncryptor();
    auto decrypted = encryptor.DecryptData(*ciphertext);
    EXPECT_TRUE(MaybeVerifyDecryptOperation(decrypted, *ciphertext));
  }
}

TEST_F(EncryptorTestWithOSCrypt, EmptyandNonEmpty) {
  // Verify that an Encryptor loaded with keys can still decrypt data encrypted
  // by an empty Encryptor. This is because OSCrypt is used for empty
  // encryptors.
  Encryptor::KeyRing key_ring_both;
  key_ring_both.emplace("TEST", GenerateRandomAES256TestKey());
  const Encryptor test_encryptor =
      GetEncryptor(std::move(key_ring_both), "TEST");

  const Encryptor encryptor = GetEncryptor();
  auto encrypted = encryptor.EncryptString("secret");
  ASSERT_TRUE(encrypted);
  auto decrypted = test_encryptor.DecryptData(*encrypted);
  ASSERT_TRUE(decrypted);
  ASSERT_EQ("secret", *decrypted);
}

TEST_F(EncryptorTestWithOSCrypt, ShortCiphertext) {
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
    auto decrypted =
        encryptor.DecryptData(base::as_bytes(base::make_span(bad_data)));
    EXPECT_TRUE(MaybeVerifyDecryptOperation(
        decrypted, base::as_bytes(base::make_span(bad_data))));
  }
}

#if BUILDFLAG(IS_WIN)

// This test verifies that data encrypted with OSCrypt can successfully be
// decrypted by an Encryptor loaded with the same key with
// Algorithm::kAES256GCM.
TEST_F(EncryptorTestBase, AlgorithmDecryptCompatibility) {
  std::string ciphertext;
  auto random_key = GenerateRandomTestKey(kKeyLength);
  // Set the OSCrypt key to the fixed key.
  OSCrypt::SetRawEncryptionKey(
      std::string(random_key.begin(), random_key.end()));

  // OSCrypt will now encrypt using this random key.
  EXPECT_TRUE(OSCrypt::EncryptString("secret", &ciphertext));

  // Reset OSCrypt so it cannot know the key, so fallback will fail.
  OSCrypt::ResetStateForTesting();
  OSCrypt::UseMockKeyForTesting(true);

  // Verify that OSCrypt can no longer decrypt this data.
  std::string plaintext;
  EXPECT_FALSE(OSCrypt::DecryptString(ciphertext, &plaintext));

  // Set up a test Encryptor that can decrypt the data.
  Encryptor::KeyRing key_ring;

  // "v10" is the OSCrypt tag for data encrypted with Algorithm::kAES256GCM. See
  // `kEncryptionVersionPrefix` in os_crypt_win.cc.
  constexpr char kEncryptionVersionPrefix[] = "v10";
  key_ring.emplace(kEncryptionVersionPrefix,
                   Encryptor::Key(random_key, mojom::Algorithm::kAES256GCM));

  // Construct an Encryptor with the same key that was used by OSCrypt to
  // encrypt the data.
  const Encryptor encryptor =
      GetEncryptor(std::move(key_ring), kEncryptionVersionPrefix);

  // The data should decrypt.
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &plaintext));
  EXPECT_EQ("secret", plaintext);

  // Reset OSCrypt for the next test.
  OSCrypt::ResetStateForTesting();
}

// This test verifies that data encrypted with an Encryptor loaded with the same
// key as OSCrypt and Algorithm::kAES256GCM can successfully be decrypted by
// OSCrypt.
TEST_F(EncryptorTestBase, AlgorithmEncryptCompatibility) {
  // From os_crypt_win.cc
  auto random_key = GenerateRandomTestKey(kKeyLength);

  // Set up a test Encryptor that can encrypt the data.
  Encryptor::KeyRing key_ring;
  // "v10" is the OSCrypt tag for data encrypted with Algorithm::kAES256GCM. See
  // `kEncryptionVersionPrefix` in os_crypt_win.cc.
  constexpr char kEncryptionVersionPrefix[] = "v10";
  key_ring.emplace(kEncryptionVersionPrefix,
                   Encryptor::Key(random_key, mojom::Algorithm::kAES256GCM));

  // Construct an Encryptor with this key. The encryption provider tag will be
  // "v10" to match OSCrypt's encryption. Encrypt the data.
  const Encryptor encryptor =
      GetEncryptor(std::move(key_ring), kEncryptionVersionPrefix);
  auto ciphertext = encryptor.EncryptString("secret");
  EXPECT_TRUE(ciphertext);

  // OSCrypt should not be able to decrypt this yet, as it does not have the
  // key.
  OSCrypt::UseMockKeyForTesting(true);
  std::string plaintext;
  EXPECT_FALSE(OSCrypt::DecryptString(
      std::string(ciphertext->begin(), ciphertext->end()), &plaintext));

  // Set the OSCrypt key to the fixed key.
  OSCrypt::ResetStateForTesting();
  OSCrypt::SetRawEncryptionKey(
      std::string(random_key.begin(), random_key.end()));

  // OSCrypt should now be able to decrypt using this key.
  EXPECT_TRUE(OSCrypt::DecryptString(
      std::string(ciphertext->begin(), ciphertext->end()), &plaintext));
  EXPECT_EQ("secret", plaintext);

  // Reset OSCrypt for the next test.
  OSCrypt::ResetStateForTesting();
}

#endif  // BUILDFLAG(IS_WIN)

class EncryptorTraitsTest : public EncryptorTestBase {};

TEST_F(EncryptorTraitsTest, TraitsRoundTrip) {
  {
    std::vector<uint8_t> test_key1(Encryptor::Key::kAES256GCMKeySize);
    crypto::RandBytes(test_key1);

    std::vector<uint8_t> test_key2(Encryptor::Key::kAES256GCMKeySize);
    crypto::RandBytes(test_key2);

    Encryptor::KeyRing key_ring;
    key_ring.emplace("TEST1",
                     Encryptor::Key(test_key1, mojom::Algorithm::kAES256GCM));
    key_ring.emplace("TEST2",
                     Encryptor::Key(test_key2, mojom::Algorithm::kAES256GCM));

    Encryptor encryptor = GetEncryptor(std::move(key_ring), "TEST1");

    Encryptor roundtripped;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Encryptor>(
        encryptor, roundtripped));

    EXPECT_EQ(roundtripped.provider_for_encryption_, "TEST1");
    EXPECT_EQ(roundtripped.keys_.size(), 2U);

    EXPECT_EQ(roundtripped.keys_.at("TEST1"),
              Encryptor::Key(test_key1, mojom::Algorithm::kAES256GCM));
    EXPECT_EQ(roundtripped.keys_.at("TEST2"),
              Encryptor::Key(test_key2, mojom::Algorithm::kAES256GCM));
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
    encryptor.keys_.at("TEST").key_.resize(8u);
    Encryptor roundtripped;

    // Mojo will fail gracefully to serialize this bad Encryptor.
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::Encryptor>(
        encryptor, roundtripped));
  }
}

}  // namespace os_crypt_async
