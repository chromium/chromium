// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {

namespace {

TEST(PrivateVerificationTokensPublicKey, Create) {
  url::Origin issuer = url::Origin::Create(GURL("https://a.example"));
  std::vector<uint8_t> public_key = {2, 3, 6, 8};
  std::vector<uint8_t> proof = {1, 2, 3, 4};
  uint32_t version = 3;
  base::Time expiration = base::Time::FromMillisecondsSinceUnixEpoch(42);
  PrivateVerificationTokensPublicKey pvt_key(issuer, public_key, proof,
                                             expiration, version);
  EXPECT_EQ(pvt_key.issuer(), issuer);
  EXPECT_EQ(pvt_key.public_key(), public_key);
  EXPECT_EQ(pvt_key.public_key_proof(), proof);
  // sha256 hash of {2,3,6,8} is
  // 0b5360a0e755a6c4b905a1b66307d6ea7715a2f2596bcb574f81fa5f58367a10
  // least significant byte in decimal is 16
  EXPECT_EQ(pvt_key.key_id(), 16);
  EXPECT_EQ(pvt_key.expiration(), expiration);
  EXPECT_EQ(pvt_key.version(), version);
}

}  // namespace

}  // namespace private_verification_tokens
