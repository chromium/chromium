// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_token.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {

namespace {

TEST(PrivateVerificationTokensToken, Create) {
  url::Origin issuer = url::Origin::Create(GURL("https://a.example"));
  SerializedToken serialized_token = {3, 5, 7, 42};
  uint32_t key_id = 7;
  base::Time expiration = base::Time::FromMillisecondsSinceUnixEpoch(27);
  uint32_t version = 3;
  PrivateVerificationTokensToken token(issuer, serialized_token, key_id,
                                       expiration, version);
  EXPECT_EQ(token.issuer(), issuer);
  EXPECT_EQ(token.token(), serialized_token);
  EXPECT_EQ(token.key_id(), key_id);
  EXPECT_EQ(token.expiration(), expiration);
  EXPECT_EQ(token.version(), version);
  EXPECT_FALSE(token.is_proxied());

  base::Time creation_time = base::Time::UnixEpoch() + base::Seconds(100);
  PrivateVerificationTokensToken unproxied_token(
      issuer, serialized_token, key_id, expiration, version, creation_time,
      /*is_proxied=*/false);
  PrivateVerificationTokensToken proxied_token(
      issuer, serialized_token, key_id, expiration, version, creation_time,
      /*is_proxied=*/true);
  EXPECT_FALSE(unproxied_token.is_proxied());
  EXPECT_TRUE(proxied_token.is_proxied());
  EXPECT_NE(unproxied_token, proxied_token);
}

}  // namespace

}  // namespace private_verification_tokens
