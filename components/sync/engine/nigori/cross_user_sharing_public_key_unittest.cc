// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(CrossUserSharingPublicKeyTest,
     GetRawCrossUserSharingPublicKeyShouldAlwaysSucceed) {
  const std::vector<uint8_t> key(X25519_PUBLIC_VALUE_LEN, 0xDE);
  const std::optional<CrossUserSharingPublicKey> public_key =
      CrossUserSharingPublicKey::CreateByImport(key);

  ASSERT_TRUE(public_key.has_value());

  const std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> raw_key =
      public_key->GetRawPublicKey();
  EXPECT_THAT(key, testing::ElementsAreArray(raw_key));
}

TEST(CrossUserSharingPublicKeyTest, CreateByImportShouldFailOnLongerKeys) {
  const std::vector<uint8_t> key(X25519_PUBLIC_VALUE_LEN * 2);
  const std::optional<CrossUserSharingPublicKey> public_key =
      CrossUserSharingPublicKey::CreateByImport(key);

  EXPECT_FALSE(public_key.has_value());
}

TEST(CrossUserSharingPublicKeyTest, CreateByImportShouldFailOnShorterKeys) {
  const std::vector<uint8_t> key(X25519_PUBLIC_VALUE_LEN - 1);
  const std::optional<CrossUserSharingPublicKey> public_key =
      CrossUserSharingPublicKey::CreateByImport(key);

  EXPECT_FALSE(public_key.has_value());
}

TEST(CrossUserSharingPublicKeyTest, CloneShouldAlwaysSucceed) {
  const std::vector<uint8_t> key(X25519_PUBLIC_VALUE_LEN, 0xDE);
  const std::optional<CrossUserSharingPublicKey> public_key =
      CrossUserSharingPublicKey::CreateByImport(key);

  std::optional<CrossUserSharingPublicKey> clone = public_key->Clone();

  ASSERT_TRUE(clone.has_value());

  const std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> raw_key =
      public_key->GetRawPublicKey();
  const std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> clone_raw_key =
      clone->GetRawPublicKey();

  EXPECT_THAT(key, testing::ElementsAreArray(raw_key));
  EXPECT_THAT(key, testing::ElementsAreArray(clone_raw_key));
}

}  // namespace
}  // namespace syncer
