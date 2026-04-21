// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

namespace {

TEST(PrivateVerificationTokensPublicKey, Create) {
  std::string etld_plus_one = "a.example";
  std::vector<uint8_t> public_key = {2, 3, 6, 8};
  uint32_t version = 3;
  uint32_t key_id = 7;
  base::Time expiration = base::Time::FromMillisecondsSinceUnixEpoch(42);
  PrivateVerificationTokensPublicKey pvt_key(etld_plus_one, public_key, key_id,
                                             expiration, version);
  EXPECT_EQ(pvt_key.etld_plus_one(), etld_plus_one);
  EXPECT_EQ(pvt_key.public_key(), public_key);
  EXPECT_EQ(pvt_key.key_id(), key_id);
  EXPECT_EQ(pvt_key.expiration(), expiration);
  EXPECT_EQ(pvt_key.version(), version);
}

}  // namespace

}  // namespace private_verification_tokens
