// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"

#include <array>

#include "base/test/gmock_expected_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::Eq;

// Valid ECDSA P-256 public key.
constexpr std::array<uint8_t, EcdsaP256PublicKey::kLength> kEcdsaP256PublicKey =
    {0x03, 0x42, 0x06, 0xc0, 0x35, 0xe5, 0x87, 0x9f, 0xdd, 0x31, 0x51,
     0x95, 0x44, 0xfd, 0x8d, 0x6c, 0x1b, 0xe9, 0x99, 0x11, 0xe8, 0x40,
     0x5b, 0xae, 0x6a, 0x36, 0x1b, 0xf5, 0x17, 0x12, 0xa1, 0x17, 0xe3};

// Invalid key that is one symbol shorter than necessary.
constexpr std::array<uint8_t, EcdsaP256PublicKey::kLength - 1>
    kInvalidShortPublicKey = {0x04, 0x42, 0x06, 0xc0, 0x35, 0xe5, 0x87, 0x9f,
                              0xdd, 0x31, 0x51, 0x95, 0x44, 0xfd, 0x8d, 0x6c,
                              0x1b, 0xe9, 0x99, 0x11, 0xe8, 0x40, 0x5b, 0xae,
                              0x6a, 0x36, 0x1b, 0xf5, 0x17, 0x12, 0xa1, 0x17};

// `kEcdsaP256PublicKey` with the first byte changed to 0x04 (invalid).
constexpr std::array<uint8_t, EcdsaP256PublicKey::kLength> kInvalidPublicKey = {
    0x04, 0x42, 0x06, 0xc0, 0x35, 0xe5, 0x87, 0x9f, 0xdd, 0x31, 0x51,
    0x95, 0x44, 0xfd, 0x8d, 0x6c, 0x1b, 0xe9, 0x99, 0x11, 0xe8, 0x40,
    0x5b, 0xae, 0x6a, 0x36, 0x1b, 0xf5, 0x17, 0x12, 0xa1, 0x17, 0xe3};

}  // namespace

TEST(EcdsaP256PublicKeyTest, Create) {
  EXPECT_THAT(EcdsaP256PublicKey::Create(kEcdsaP256PublicKey), HasValue());

  EXPECT_THAT(EcdsaP256PublicKey::Create(kInvalidShortPublicKey),
              ErrorIs(Eq("The ECDSA P-256 public key does not have the correct "
                         "length. Expected 33 bytes, but received 32 bytes.")));

  EXPECT_THAT(
      EcdsaP256PublicKey::Create(kInvalidPublicKey),
      ErrorIs(
          Eq("Unable to parse a valid ECDSA P-256 key from the given bytes.")));
}

}  // namespace web_package
