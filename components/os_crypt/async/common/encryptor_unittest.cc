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
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
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

}  // namespace

class EncryptorTestBase : public ::testing::Test {
 protected:
  // This is here so it can access the private methods in Encryptor via friends.
  const Encryptor GetTestEncryptor() { return Encryptor(); }
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

using EncryptorTest = EncryptorTestWithOSCrypt;

TEST_F(EncryptorTest, StringInterface) {
  const Encryptor encryptor = GetTestEncryptor();
  std::string plaintext = "secrets";
  std::string ciphertext;
  EXPECT_TRUE(encryptor.EncryptString(plaintext, &ciphertext));
  std::string decrypted;
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(EncryptorTest, SpanInterface) {
  const Encryptor encryptor = GetTestEncryptor();
  std::string plaintext = "secrets";

  auto ciphertext = encryptor.EncryptString(plaintext);
  ASSERT_TRUE(ciphertext);

  auto decrypted = encryptor.DecryptData(*ciphertext);

  ASSERT_TRUE(decrypted);

  EXPECT_EQ(plaintext, *decrypted);
}

TEST_F(EncryptorTest, EncryptStringDecryptSpan) {
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

TEST_F(EncryptorTest, EncryptSpanDecryptString) {
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

TEST_F(EncryptorTest, EncryptEmpty) {
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
TEST_F(EncryptorTest, DecryptEmpty) {
  const Encryptor encryptor = GetTestEncryptor();

  auto plaintext = encryptor.DecryptData({});
  ASSERT_TRUE(plaintext);
  EXPECT_TRUE(plaintext->empty());
}

// Non-Windows platforms can decrypt random data fine.
#if BUILDFLAG(IS_WIN)
TEST_F(EncryptorTest, DecryptInvalid) {
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
TEST_F(EncryptorTest, DecryptFallback) {
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
TEST_F(EncryptorTest, AncientFallback) {
  std::string ciphertext;
  EXPECT_TRUE(EncryptStringWithDPAPI("secret", ciphertext));

  std::string decrypted;
  const Encryptor encryptor = GetTestEncryptor();
  // Encryptor can still decrypt very old DPAPI data.
  EXPECT_TRUE(encryptor.DecryptString(ciphertext, &decrypted));

  EXPECT_EQ("secret", decrypted);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace os_crypt_async
