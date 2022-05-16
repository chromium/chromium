// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/os_crypt/os_crypt_mocker_linux.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/prefs/testing_pref_service.h"
#include "crypto/random.h"
#endif

namespace {

class OSCryptTest : public testing::Test {
 protected:
  OSCrypt os_crypt_;
  OSCryptMocker os_crypt_mocker_{&os_crypt_};
};

TEST_F(OSCryptTest, String16EncryptionDecryption) {
  std::u16string plaintext;
  std::u16string result;
  std::string utf8_plaintext;
  std::string utf8_result;
  std::string ciphertext;

  // Test borderline cases (empty strings).
  EXPECT_TRUE(os_crypt_.EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(os_crypt_.DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test a simple string.
  plaintext = u"hello";
  EXPECT_TRUE(os_crypt_.EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(os_crypt_.DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test a 16-byte aligned string.  This previously hit a boundary error in
  // base::OSCrypt::Crypt() on Mac.
  plaintext = u"1234567890123456";
  EXPECT_TRUE(os_crypt_.EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(os_crypt_.DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test Unicode.
  char16_t wchars[] = {0xdbeb, 0xdf1b, 0x4e03, 0x6708, 0x8849, 0x661f, 0x671f,
                       0x56db, 0x597c, 0x4e03, 0x6708, 0x56db, 0x6708, 0xe407,
                       0xdbaf, 0xdeb5, 0x4ec5, 0x544b, 0x661f, 0x671f, 0x65e5,
                       0x661f, 0x671f, 0x4e94, 0xd8b1, 0xdce1, 0x7052, 0x5095,
                       0x7c0b, 0xe586, 0};
  plaintext = wchars;
  utf8_plaintext = base::UTF16ToUTF8(plaintext);
  EXPECT_EQ(plaintext, base::UTF8ToUTF16(utf8_plaintext));
  EXPECT_TRUE(os_crypt_.EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(os_crypt_.DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);
  EXPECT_TRUE(os_crypt_.DecryptString(ciphertext, &utf8_result));
  EXPECT_EQ(utf8_plaintext, base::UTF16ToUTF8(result));

  EXPECT_TRUE(os_crypt_.EncryptString(utf8_plaintext, &ciphertext));
  EXPECT_TRUE(os_crypt_.DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);
  EXPECT_TRUE(os_crypt_.DecryptString(ciphertext, &utf8_result));
  EXPECT_EQ(utf8_plaintext, base::UTF16ToUTF8(result));
}

TEST_F(OSCryptTest, EncryptionDecryption) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // Test borderline cases (empty strings).
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test a simple string.
  plaintext = "hello";
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Make sure it null terminates.
  plaintext.assign("hello", 3);
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, "hel");
}

TEST_F(OSCryptTest, CypherTextDiffers) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // Test borderline cases (empty strings).
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &result));
  // |ciphertext| is empty on the Mac, different on Windows.
  EXPECT_TRUE(ciphertext.empty() || plaintext != ciphertext);
  EXPECT_EQ(plaintext, result);

  // Test a simple string.
  plaintext = "hello";
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &result));
  EXPECT_NE(plaintext, ciphertext);
  EXPECT_EQ(plaintext, result);

  // Make sure it null terminates.
  plaintext.assign("hello", 3);
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &result));
  EXPECT_NE(plaintext, ciphertext);
  EXPECT_EQ(result, "hel");
}

TEST_F(OSCryptTest, DecryptError) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // Test a simple string, messing with ciphertext prior to decrypting.
  plaintext = "hello";
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));
  EXPECT_NE(plaintext, ciphertext);
  ASSERT_LT(4UL, ciphertext.size());
  ciphertext[3] = ciphertext[3] + 1;
  EXPECT_FALSE(os_crypt_.DecryptString(ciphertext, &result));
  EXPECT_NE(plaintext, result);
  EXPECT_TRUE(result.empty());
}

class OSCryptConcurrencyTest : public testing::Test {
 public:
  void AssertProperEncryption() {
    std::string plaintext = "secrets";
    std::string encrypted;
    std::string decrypted;
    ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &encrypted));
    ASSERT_TRUE(os_crypt_.DecryptString(encrypted, &decrypted));
    ASSERT_EQ(plaintext, decrypted);
  }

 protected:
  OSCrypt os_crypt_;
  OSCryptMocker os_crypt_mocker_{&os_crypt_};
};

// Flaky on Win 7 (dbg) and win-asan, see https://crbug.com/1066699
#if BUILDFLAG(IS_WIN)
#define MAYBE_ConcurrentInitialization DISABLED_ConcurrentInitialization
#else
#define MAYBE_ConcurrentInitialization ConcurrentInitialization
#endif
TEST_F(OSCryptConcurrencyTest, MAYBE_ConcurrentInitialization) {
  // Launch multiple threads
  base::Thread thread1("thread1");
  base::Thread thread2("thread2");
  std::vector<base::Thread*> threads = {&thread1, &thread2};
  for (base::Thread* thread : threads) {
    ASSERT_TRUE(thread->Start());
  }

  // Make calls.
  for (base::Thread* thread : threads) {
    ASSERT_TRUE(thread->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&OSCryptConcurrencyTest::AssertProperEncryption,
                       base::Unretained(this))));
  }

  // Cleanup
  for (base::Thread* thread : threads) {
    thread->Stop();
  }
}

#if BUILDFLAG(IS_WIN)

class OSCryptTestWin : public testing::Test {
 public:
  OSCryptTestWin() {}

  OSCryptTestWin(const OSCryptTestWin&) = delete;
  OSCryptTestWin& operator=(const OSCryptTestWin&) = delete;

  ~OSCryptTestWin() override { os_crypt_mocker_.ResetState(); }

 protected:
  OSCrypt os_crypt_;
  OSCryptMocker os_crypt_mocker_{&os_crypt_};
};

// This test verifies that the header of the data returned from CryptProtectData
// never collides with the kEncryptionVersionPrefix ("v10") used in
// os_crypt_win.cc. If this ever happened, we would not be able to distinguish
// between data encrypted using the legacy DPAPI interface, and data that's been
// encrypted with the new session key.

// If this test ever breaks do not ignore it as it might result in data loss for
// users.
TEST_F(OSCryptTestWin, DPAPIHeader) {
  std::string plaintext;
  std::string ciphertext;

  os_crypt_mocker_.SetLegacyEncryption(true);
  crypto::RandBytes(base::WriteInto(&plaintext, 11), 10);
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));

  using std::string_literals::operator""s;
  const std::string expected_header("\x01\x00\x00\x00"s);
  ASSERT_EQ(4U, expected_header.length());

  ASSERT_TRUE(ciphertext.length() >= expected_header.length());
  std::string dpapi_header = ciphertext.substr(0, expected_header.length());
  EXPECT_EQ(expected_header, dpapi_header);
}

TEST_F(OSCryptTestWin, ReadOldData) {
  os_crypt_mocker_.SetLegacyEncryption(true);

  std::string plaintext = "secrets";
  std::string legacy_ciphertext;
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &legacy_ciphertext));

  os_crypt_mocker_.SetLegacyEncryption(false);

  TestingPrefServiceSimple pref_service_simple;
  OSCrypt::RegisterLocalPrefs(pref_service_simple.registry());
  ASSERT_TRUE(os_crypt_.Init(&pref_service_simple));

  std::string decrypted;
  // Should be able to decrypt data encrypted with DPAPI.
  ASSERT_TRUE(os_crypt_.DecryptString(legacy_ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);

  // Should now encrypt same plaintext to get different ciphertext.
  std::string new_ciphertext;
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &new_ciphertext));

  // Should be different from DPAPI ciphertext.
  EXPECT_NE(legacy_ciphertext, new_ciphertext);

  // Decrypt new ciphertext to give original string.
  ASSERT_TRUE(os_crypt_.DecryptString(new_ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(OSCryptTestWin, PrefsKeyTest) {
#if BUILDFLAG(IS_WIN)
  // Real use scenario. Disable os crypt mocking.
  os_crypt_.UseMockKeyForTesting(false);
#endif  // BUILDFLAG(IS_WIN)

  TestingPrefServiceSimple first_prefs;
  OSCrypt::RegisterLocalPrefs(first_prefs.registry());

  // Verify new random key can be generated.
  ASSERT_TRUE(os_crypt_.Init(&first_prefs));
  std::string first_key = os_crypt_.GetRawEncryptionKey();

  std::string plaintext = "secrets";
  std::string ciphertext;
  ASSERT_TRUE(os_crypt_.EncryptString(plaintext, &ciphertext));

  TestingPrefServiceSimple second_prefs;
  os_crypt_.RegisterLocalPrefs(second_prefs.registry());

  os_crypt_mocker_.ResetState();
  ASSERT_TRUE(os_crypt_.Init(&second_prefs));
  std::string second_key = os_crypt_.GetRawEncryptionKey();
  // Keys should be different since they are random.
  EXPECT_NE(first_key, second_key);

  std::string decrypted;
  // Cannot decrypt with the wrong key.
  EXPECT_FALSE(os_crypt_.DecryptString(ciphertext, &decrypted));

  // Initialize OSCrypt from existing key.
  os_crypt_mocker_.ResetState();
  os_crypt_.SetRawEncryptionKey(first_key);

  // Verify decryption works with first key.
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);

  // Initialize OSCrypt from existing prefs.
  os_crypt_mocker_.ResetState();
  ASSERT_TRUE(os_crypt_.Init(&first_prefs));

  // Verify decryption works with key from first prefs.
  ASSERT_TRUE(os_crypt_.DecryptString(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace
