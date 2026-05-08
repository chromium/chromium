// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_token.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

namespace {

TEST(PrivateVerificationTokensToken, Create) {
  std::string etld_plus_one = "a.example";
  SerializedToken serialized_token = {3, 5, 7, 42};
  uint32_t key_id = 7;
  base::Time expiration = base::Time::FromMillisecondsSinceUnixEpoch(27);
  uint32_t version = 3;
  PrivateVerificationTokensToken token(etld_plus_one, serialized_token, key_id,
                                       expiration, version);
  EXPECT_EQ(token.etld_plus_one(), etld_plus_one);
  EXPECT_EQ(token.token(), serialized_token);
  EXPECT_EQ(token.key_id(), key_id);
  EXPECT_EQ(token.expiration(), expiration);
  EXPECT_EQ(token.version(), version);
}

}  // namespace

}  // namespace private_verification_tokens
