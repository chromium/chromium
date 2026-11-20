// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_key_and_version.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

TEST(TrustedVaultKeyAndVersionTest, Equality) {
  const TrustedVaultKeyAndVersion key1({1, 2, 3}, /*version=*/5);
  const TrustedVaultKeyAndVersion key1_copy({1, 2, 3}, /*version=*/5);
  const TrustedVaultKeyAndVersion different_key({1, 2, 4}, /*version=*/5);
  const TrustedVaultKeyAndVersion different_version({1, 2, 3}, /*version=*/6);

  EXPECT_EQ(key1, key1_copy);
  EXPECT_NE(key1, different_key);
  EXPECT_NE(key1, different_version);
}

TEST(TrustedVaultKeyAndVersionTest, GetTrustedVaultKeysWithVersionsEmpty) {
  EXPECT_THAT(GetTrustedVaultKeysWithVersions(/*trusted_vault_keys=*/{},
                                              /*last_key_version=*/10),
              IsEmpty());
  EXPECT_THAT(GetTrustedVaultKeysWithVersions(
                  /*trusted_vault_keys=*/{},
                  /*last_key_version=*/std::numeric_limits<int>::max()),
              IsEmpty());
}

TEST(TrustedVaultKeyAndVersionTest, GetTrustedVaultKeysWithVersionsSingleKey) {
  const std::vector<uint8_t> kKey = {1, 2, 3, 4};
  EXPECT_THAT(GetTrustedVaultKeysWithVersions({kKey}, /*last_key_version=*/7),
              ElementsAre(TrustedVaultKeyAndVersion(kKey, 7)));
}

TEST(TrustedVaultKeyAndVersionTest,
     GetTrustedVaultKeysWithVersionsMultipleSequentialKeys) {
  const std::vector<uint8_t> kKey1 = {1, 2};
  const std::vector<uint8_t> kKey2 = {3, 4};
  const std::vector<uint8_t> kKey3 = {5, 6};
  EXPECT_THAT(GetTrustedVaultKeysWithVersions({kKey1, kKey2, kKey3},
                                              /*last_key_version=*/12),
              ElementsAre(TrustedVaultKeyAndVersion(kKey1, 10),
                          TrustedVaultKeyAndVersion(kKey2, 11),
                          TrustedVaultKeyAndVersion(kKey3, 12)));
}

}  // namespace
}  // namespace trusted_vault
