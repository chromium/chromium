// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// * This code is taken from base/sha1, with small changes.
//------------------------------------------------------------------------------

#include "chrome/chrome_elf/sha1/sha1.h"

#include <stddef.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SHA1Test, Test1) {
  // Example A.1 from FIPS 180-2: one-block message.
  std::string input = "abc";

  elf_sha1::Digest expected = {0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81,
                               0x6a, 0xba, 0x3e, 0x25, 0x71, 0x78, 0x50,
                               0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d};

  EXPECT_EQ(elf_sha1::SHA1HashString(input), expected);
}

TEST(SHA1Test, Test2) {
  // Example A.2 from FIPS 180-2: multi-block message.
  std::string input =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

  elf_sha1::Digest expected = {0x84, 0x98, 0x3e, 0x44, 0x1c, 0x3b, 0xd2,
                               0x6e, 0xba, 0xae, 0x4a, 0xa1, 0xf9, 0x51,
                               0x29, 0xe5, 0xe5, 0x46, 0x70, 0xf1};

  EXPECT_EQ(elf_sha1::SHA1HashString(input), expected);
}

TEST(SHA1Test, Test3) {
  // Example A.3 from FIPS 180-2: long message.
  std::string input(1000000, 'a');

  elf_sha1::Digest expected = {0x34, 0xaa, 0x97, 0x3c, 0xd4, 0xc4, 0xda,
                               0xa4, 0xf6, 0x1e, 0xeb, 0x2b, 0xdb, 0xad,
                               0x27, 0x31, 0x65, 0x34, 0x01, 0x6f};

  EXPECT_EQ(elf_sha1::SHA1HashString(input), expected);
}

}  // namespace
