// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/internal/platform/implementation/crypto.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace nearby {

TEST(CryptoTest, Md5GeneratesHash) {
  const ByteArray expected_md5(
      "\xb4\x5c\xff\xe0\x84\xdd\x3d\x20\xd9\x28\xbe\xe8\x5e\x7b\x0f\x21");
  ByteArray md5_hash = Crypto::Md5("string");
  EXPECT_EQ(md5_hash, expected_md5);
}

TEST(CryptoTest, Md5ReturnsEmptyOnError) {
  EXPECT_EQ(Crypto::Md5(""), ByteArray{});
}

TEST(CryptoTest, Sha256GeneratesHash) {
  const ByteArray expected_sha256(
      "\x47\x32\x87\xf8\x29\x8d\xba\x71\x63\xa8\x97\x90\x89\x58\xf7\xc0"
      "\xea\xe7\x33\xe2\x5d\x2e\x02\x79\x92\xea\x2e\xdc\x9b\xed\x2f\xa8");
  ByteArray sha256_hash = Crypto::Sha256("string");
  EXPECT_EQ(sha256_hash, expected_sha256);
}

TEST(CryptoTest, Sha256ReturnsEmptyOnError) {
  EXPECT_EQ(Crypto::Sha256(""), ByteArray{});
}

}  // namespace nearby
