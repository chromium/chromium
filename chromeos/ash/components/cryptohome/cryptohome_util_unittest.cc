// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/cryptohome_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cryptohome {

using CryptohomeUtilTest = ::testing::Test;

TEST_F(CryptohomeUtilTest, GetCryptohomeId) {
  // UNKNOWN type
  AccountId account_id1 = AccountId::FromUserEmail("user1@test.com");
  // GOOGLE type
  AccountId account_id2 =
      AccountId::FromUserEmailGaiaId("user2@test.com", "1234567890-1");
  // ACTIVE_DIRECTORY type
  AccountId account_id3 =
      AccountId::AdFromUserEmailObjGuid("user3@test.com", "obj-guid");

  EXPECT_EQ(GetCryptohomeId(account_id1), "user1@test.com");
  EXPECT_EQ(GetCryptohomeId(account_id2), "user2@test.com");
  EXPECT_EQ(GetCryptohomeId(account_id3), "a-obj-guid");
}

}  // namespace cryptohome
