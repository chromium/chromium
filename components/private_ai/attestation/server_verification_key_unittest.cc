// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/attestation/server_verification_key.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/private_ai/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

class ServerVerificationKeyTest : public ::testing::Test {
 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServerVerificationKeyTest, GetAutopushKeys) {
  GURL url("https://autopush-private-ai.example.com");
  auto keys = GetServerVerificationKey(url);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetDevKeysForTesting());
  EXPECT_NE(keys, GetStagingKeysForTesting());
}

TEST_F(ServerVerificationKeyTest, GetDevKeys) {
  GURL url("https://dev-private-ai.example.com");
  auto keys = GetServerVerificationKey(url);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetDevKeysForTesting());
  EXPECT_NE(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetStagingKeysForTesting());
}

TEST_F(ServerVerificationKeyTest, GetProdKeys) {
  GURL url("https://private-ai.example.com");
  auto keys = GetServerVerificationKey(url);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetAutopushKeysForTesting());
  EXPECT_EQ(keys, GetProdKeysForTesting());
  EXPECT_NE(keys, GetDevKeysForTesting());
  EXPECT_NE(keys, GetStagingKeysForTesting());
}

TEST_F(ServerVerificationKeyTest, GetStagingKeys) {
  GURL url("https://staging-private-ai.example.com");
  auto keys = GetServerVerificationKey(url);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetStagingKeysForTesting());
  EXPECT_NE(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetDevKeysForTesting());
}

}  // namespace private_ai
