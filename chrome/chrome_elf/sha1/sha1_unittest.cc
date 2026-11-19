// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/sha1/sha1.h"

#include <stddef.h>

#include <string>

#include "base/containers/span.h"
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

// Verify that the implementation matches known-good hashes across input
// lengths, especially around the 55/56-byte padding threshold (where the
// 8-byte bit length spills into an extra block) and the 64-byte block
// boundary.
TEST(SHA1Test, LengthBoundaryConditions) {
  static const struct {
    size_t bytes;
    elf_sha1::Digest digest;
  } cases[] = {
      // These test vectors can be regenerated, if you need to, with:
      //   perl -e 'print("a" x N)' | sha1sum
      {55,
       {
           0xc1, 0xc8, 0xbb, 0xdc, 0x22, 0x79, 0x6e, 0x28, 0xc0, 0xe1,
           0x51, 0x63, 0xd2, 0x08, 0x99, 0xb6, 0x56, 0x21, 0xd6, 0x5a,
       }},
      {56,
       {
           0xc2, 0xdb, 0x33, 0x0f, 0x60, 0x83, 0x85, 0x4c, 0x99, 0xd4,
           0xb5, 0xbf, 0xb6, 0xe8, 0xf2, 0x9f, 0x20, 0x1b, 0xe6, 0x99,
       }},
      {64,
       {
           0x00, 0x98, 0xba, 0x82, 0x4b, 0x5c, 0x16, 0x42, 0x7b, 0xd7,
           0xa1, 0x12, 0x2a, 0x5a, 0x44, 0x2a, 0x25, 0xec, 0x64, 0x4d,
       }},
      {65,
       {
           0x11, 0x65, 0x53, 0x26, 0xc7, 0x08, 0xd7, 0x03, 0x19, 0xbe,
           0x26, 0x10, 0xe8, 0xa5, 0x7d, 0x9a, 0x5b, 0x95, 0x9d, 0x3b,
       }},
  };

  for (const auto& c : cases) {
    const auto digest = elf_sha1::SHA1HashString(std::string(c.bytes, 'a'));
    EXPECT_EQ(digest, c.digest);
  }
}

}  // namespace
