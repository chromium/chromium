// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
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
  base::win::ScopedLocalAlloc scoped_memory(output.pbData);

  ciphertext.assign(reinterpret_cast<std::string::value_type*>(output.pbData),
                    output.cbData);

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class OSCryptAsyncTest : public ::testing::Test {
 protected:
  void SetUp() override { OSCryptMocker::SetUp(); }

  void TearDown() override {
    OSCryptMocker::TearDown();
#if BUILDFLAG(IS_WIN)
    OSCrypt::ResetStateForTesting();
#endif  // BUILDFLAG(IS_WIN)
  }

  std::unique_ptr<Encryptor> GetInstanceSync() {
    base::RunLoop run_loop;
    std::unique_ptr<Encryptor> encryptor;
    auto sub = factory_.GetInstance(base::BindLambdaForTesting(
        [&](Encryptor encryptor_param, bool success) {
          EXPECT_TRUE(success);
          encryptor = std::make_unique<Encryptor>(std::move(encryptor_param));
          run_loop.Quit();
        }));
    run_loop.Run();
    return encryptor;
  }

  OSCryptAsync factory_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(OSCryptAsyncTest, StringInterface) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);
  std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(encryptor->EncryptString(plaintext, &ciphertext));
  std::string decrypted;
  ASSERT_TRUE(encryptor->DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(OSCryptAsyncTest, SpanInterface) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);
  std::string plaintext = "secrets";

  auto ciphertext = encryptor->EncryptString(plaintext);
  ASSERT_TRUE(ciphertext.has_value());

  auto decrypted = encryptor->DecryptData(*ciphertext);

  ASSERT_TRUE(decrypted.has_value());

  EXPECT_EQ(plaintext, *decrypted);
}

TEST_F(OSCryptAsyncTest, EncryptStringDecryptSpan) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);
  std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(encryptor->EncryptString(plaintext, &ciphertext));

  auto decrypted =
      encryptor->DecryptData(base::as_bytes(base::make_span(ciphertext)));

  ASSERT_TRUE(decrypted.has_value());

  ASSERT_EQ(plaintext.size(), decrypted->size());

  ASSERT_TRUE(
      std::equal(plaintext.cbegin(), plaintext.cend(), decrypted->cbegin()));
}

TEST_F(OSCryptAsyncTest, EncryptSpanDecryptString) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);
  std::string plaintext = "secrets";

  auto ciphertext = encryptor->EncryptString(plaintext);
  ASSERT_TRUE(ciphertext.has_value());

  std::string decrypted;
  ASSERT_TRUE(encryptor->DecryptString(
      std::string(ciphertext->begin(), ciphertext->end()), &decrypted));
  ASSERT_EQ(plaintext.size(), decrypted.size());

  ASSERT_TRUE(
      std::equal(plaintext.cbegin(), plaintext.cend(), decrypted.cbegin()));
}

TEST_F(OSCryptAsyncTest, EncryptEmpty) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);

  auto ciphertext = encryptor->EncryptString(std::string());
  ASSERT_TRUE(ciphertext.has_value());

  auto decrypted = encryptor->DecryptData(*ciphertext);
  ASSERT_TRUE(decrypted);
  ASSERT_TRUE(decrypted->empty());
}

// In a behavior change on Windows, Decrypt/Encrypt of empty data results in a
// success and an empty buffer. This was already the behavior on non-Windows so
// this change makes it consistent.
TEST_F(OSCryptAsyncTest, DecryptEmpty) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);

  auto plaintext = encryptor->DecryptData({});
  ASSERT_TRUE(plaintext);
  ASSERT_TRUE(plaintext->empty());
}

// Non-Windows platforms can decrypt random data fine.
#if BUILDFLAG(IS_WIN)
TEST_F(OSCryptAsyncTest, DecryptInvalid) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();
  ASSERT_TRUE(encryptor);

  std::vector<uint8_t> invalid_cipher(100);
  for (auto c : invalid_cipher) {
    invalid_cipher[c] = c;
  }

  auto plaintext = encryptor->DecryptData(invalid_cipher);
  ASSERT_FALSE(plaintext.has_value());
}
#endif  // BUILDFLAG(IS_WIN)

// This test verifies that GetInstanceAsync can correctly handle multiple queued
// requests for an instance for a slow init.
TEST_F(OSCryptAsyncTest, MultipleCalls) {
  size_t calls = 0;
  const size_t kExpectedCalls = 10;
  base::RunLoop run_loop;
  std::list<base::CallbackListSubscription> subs;
  for (size_t call = 0; call < kExpectedCalls; call++) {
    subs.push_back(factory_.GetInstance(base::BindLambdaForTesting(
        [&calls, &run_loop](Encryptor encryptor, bool success) {
          calls++;
          if (calls == kExpectedCalls) {
            run_loop.Quit();
          }
        })));
  }
  run_loop.Run();
  EXPECT_EQ(calls, kExpectedCalls);
}

// Encryptor can decrypt data encrypted with OSCrypt.
TEST_F(OSCryptAsyncTest, DecryptFallback) {
  std::string ciphertext;
  EXPECT_TRUE(OSCrypt::EncryptString("secret", &ciphertext));

  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();

  std::string decrypted;
  // Fallback to OSCrypt takes place.
  EXPECT_TRUE(encryptor->DecryptString(ciphertext, &decrypted));

  EXPECT_EQ("secret", decrypted);
}

#if BUILDFLAG(IS_WIN)
// Encryptor should still decrypt data encrypted using DPAPI (pre-m79) by fall
// back to OSCrypt.
TEST_F(OSCryptAsyncTest, AncientFallback) {
  std::unique_ptr<Encryptor> encryptor = GetInstanceSync();

  std::string ciphertext;
  EXPECT_TRUE(EncryptStringWithDPAPI("secret", ciphertext));

  std::string decrypted;
  // Encryptor can still decrypt very old DPAPI data.
  EXPECT_TRUE(encryptor->DecryptString(ciphertext, &decrypted));

  EXPECT_EQ("secret", decrypted);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace os_crypt_async
