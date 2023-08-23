// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr std::array<uint8_t, 32> kPublicKey1 = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 32> kPublicKey2 = {
    0x02, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};
}  // namespace

TEST(Ed25519PublicKeyTest, ValidPublicKey) {
  const auto public_key1 =
      Ed25519PublicKey::Create(base::make_span(kPublicKey1));
  EXPECT_EQ(public_key1.bytes(), kPublicKey1);
}

TEST(Ed25519PublicKeyTest, Comparators) {
  const auto public_key1 =
      Ed25519PublicKey::Create(base::make_span(kPublicKey1));
  const auto public_key2 =
      Ed25519PublicKey::Create(base::make_span(kPublicKey2));

  EXPECT_TRUE(public_key1 == public_key1);
  EXPECT_TRUE(public_key1 != public_key2);

  EXPECT_FALSE(public_key1 == public_key2);
  EXPECT_FALSE(public_key1 != public_key1);
}

TEST(Ed25519PublicKeyTest, InvalidPublicKey) {
  std::vector<uint8_t> invalid_public_key(std::begin(kPublicKey1),
                                          std::end(kPublicKey1));
  // Make the key one byte too long.
  invalid_public_key.push_back(0xff);

  EXPECT_FALSE(Ed25519PublicKey::Create(invalid_public_key).has_value());
}

}  // namespace web_package
