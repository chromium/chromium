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
  base::FieldTrialParams params;
  params["url"] = "autopush-private-ai.corp.google.com";
  feature_list_.InitAndEnableFeatureWithParameters(kPrivateAi, params);

  auto keys = GetServerVerificationKey();
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetDevKeysForTesting());
  EXPECT_NE(keys, GetStagingKeysForTesting());
}

TEST_F(ServerVerificationKeyTest, GetDevKeys) {
  base::FieldTrialParams params;
  params["url"] = "dev-private-ai.corp.google.com";
  feature_list_.InitAndEnableFeatureWithParameters(kPrivateAi, params);

  auto keys = GetServerVerificationKey();
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetDevKeysForTesting());
  EXPECT_NE(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetStagingKeysForTesting());
}

TEST_F(ServerVerificationKeyTest, GetProdKeys) {
  base::FieldTrialParams params;
  params["url"] = "private-ai.corp.google.com";
  feature_list_.InitAndEnableFeatureWithParameters(kPrivateAi, params);

  auto keys = GetServerVerificationKey();
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetProdKeysForTesting());
  EXPECT_NE(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetDevKeysForTesting());
  EXPECT_NE(keys, GetStagingKeysForTesting());
}

TEST_F(ServerVerificationKeyTest, GetStagingKeys) {
  base::FieldTrialParams params;
  params["url"] = "staging-private-ai.corp.google.com";
  feature_list_.InitAndEnableFeatureWithParameters(kPrivateAi, params);

  auto keys = GetServerVerificationKey();
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys, GetStagingKeysForTesting());
  EXPECT_NE(keys, GetAutopushKeysForTesting());
  EXPECT_NE(keys, GetDevKeysForTesting());
}

}  // namespace private_ai
