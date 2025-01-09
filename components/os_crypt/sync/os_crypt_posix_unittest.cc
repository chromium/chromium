// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <array>
#include <string>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(OSCryptPosixTest, EncryptDecryptKnownAnswers) {
  const char kInput[] = "Hello, World! This is a longer string.";

  // The input string, encrypted using AES-128-CBC, with the "v10" version
  // prefix and with a PKCS#5 padding block at the end. The encryption key is
  // the hardcoded v10 obfuscation key.
  // clang-format off
  constexpr auto kExpectedCiphertext = std::to_array<uint8_t>({
    0x76, 0x31, 0x30,
    0x1c, 0x67, 0x22, 0xf7, 0x3a, 0x1c, 0xdd, 0xf0,
    0x9a, 0x4f, 0x0f, 0x15, 0x9f, 0x23, 0x41, 0x49,
    0x70, 0x76, 0xc4, 0x36, 0xf1, 0xee, 0x7b, 0xd3,
    0x21, 0x3c, 0x0e, 0x60, 0x47, 0xd3, 0x31, 0x6e,
    0x31, 0xb5, 0x99, 0xef, 0x1f, 0x8a, 0xa0, 0xac,
    0x97, 0xd9, 0x45, 0x85, 0x17, 0xb0, 0xcd, 0x1a,
  });
  // clang-format on

  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString(kInput, &ciphertext));
  EXPECT_EQ(kExpectedCiphertext, base::as_byte_span(ciphertext));

  std::string plaintext;
  ASSERT_TRUE(OSCrypt::DecryptString(ciphertext, &plaintext));
  EXPECT_EQ(kInput, plaintext);
}

}  // namespace
