// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dm_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyDMTokenTest, Empty) {
  const policy::DMToken token = DMToken::CreateEmptyToken();
  EXPECT_TRUE(token.is_empty());
  EXPECT_FALSE(token.is_invalid());
  EXPECT_FALSE(token.is_valid());
}

TEST(PolicyDMTokenTest, Invalid) {
  const policy::DMToken token = DMToken::CreateInvalidToken();
  EXPECT_FALSE(token.is_empty());
  EXPECT_TRUE(token.is_invalid());
  EXPECT_FALSE(token.is_valid());
}

TEST(PolicyDMTokenTest, Valid) {
  constexpr char value[] = "whatever";
  const policy::DMToken token = DMToken::CreateValidToken(value);
  EXPECT_FALSE(token.is_empty());
  EXPECT_FALSE(token.is_invalid());
  EXPECT_TRUE(token.is_valid());
  EXPECT_EQ(token.value(), value);
}

}  // namespace policy
