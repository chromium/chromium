// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cross_user_sharing_keys.h"

#include <vector>

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Eq;
using testing::Ne;
using testing::SizeIs;

TEST(CrossUserSharingKeysTest, ShouldCreateEmpty) {
  const CrossUserSharingKeys keys = CrossUserSharingKeys::CreateEmpty();
  EXPECT_THAT(keys, SizeIs(0));
  EXPECT_FALSE(keys.HasKeyPair(0));
}

TEST(CrossUserSharingKeysTest, ShouldConvertEmptyToProto) {
  EXPECT_EQ(sync_pb::CrossUserSharingKeys().SerializeAsString(),
            CrossUserSharingKeys::CreateEmpty().ToProto().SerializeAsString());
}

TEST(CrossUserSharingKeysTest, ShouldCreateEmptyFromProto) {
  EXPECT_THAT(
      CrossUserSharingKeys::CreateFromProto(sync_pb::CrossUserSharingKeys()),
      SizeIs(0));
}

TEST(CrossUserSharingKeysTest, ShouldCreateNonEmptyFromProto) {
  CrossUserSharingKeys original_keys = CrossUserSharingKeys::CreateEmpty();
  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  ASSERT_THAT(original_keys, SizeIs(2));

  const CrossUserSharingKeys restored_keys =
      CrossUserSharingKeys::CreateFromProto(original_keys.ToProto());
  EXPECT_THAT(restored_keys, SizeIs(2));
  EXPECT_TRUE(restored_keys.HasKeyPair(0));
  EXPECT_TRUE(restored_keys.HasKeyPair(1));
}

TEST(CrossUserSharingKeysTest, ShouldCreateNonEmptyFromPartiallyInvalidProto) {
  CrossUserSharingKeys original_keys = CrossUserSharingKeys::CreateEmpty();
  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  sync_pb::CrossUserSharingKeys malformed_proto = original_keys.ToProto();
  ASSERT_THAT(malformed_proto.private_key(), SizeIs(2));
  malformed_proto.mutable_private_key(1)->set_x25519_private_key(
      "malformed-key");

  CrossUserSharingKeys restored_keys =
      CrossUserSharingKeys::CreateFromProto(malformed_proto);

  EXPECT_THAT(restored_keys, SizeIs(1));
  EXPECT_TRUE(restored_keys.HasKeyPair(0));
  EXPECT_FALSE(restored_keys.HasKeyPair(1));
}

TEST(CrossUserSharingKeysTest, ShouldCloneWithKeyPairs) {
  CrossUserSharingKeys original_keys = CrossUserSharingKeys::CreateEmpty();

  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  const CrossUserSharingKeys cloned_keys = original_keys.Clone();

  EXPECT_TRUE(cloned_keys.HasKeyPair(0));
  EXPECT_TRUE(cloned_keys.HasKeyPair(1));
}

TEST(CrossUserSharingKeysTest, ShouldSetKeyPair) {
  CrossUserSharingKeys keys = CrossUserSharingKeys::CreateEmpty();

  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  0);

  EXPECT_TRUE(keys.HasKeyPair(0));
}

TEST(CrossUserSharingKeysTest, ShouldAddMultipleKeyPairs) {
  CrossUserSharingKeys keys = CrossUserSharingKeys::CreateEmpty();

  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  0);
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  1);
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  3);

  EXPECT_TRUE(keys.HasKeyPair(0));
  EXPECT_TRUE(keys.HasKeyPair(1));
  EXPECT_TRUE(keys.HasKeyPair(3));
}

TEST(CrossUserSharingKeysTest, ShouldCreateNonEmptyKeyPairsFromProto) {
  CrossUserSharingKeys original_keys = CrossUserSharingKeys::CreateEmpty();

  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 0);
  original_keys.SetKeyPair(
      CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(), 1);

  const CrossUserSharingKeys restored_keys =
      CrossUserSharingKeys::CreateFromProto(original_keys.ToProto());

  EXPECT_TRUE(restored_keys.HasKeyPair(0));
  EXPECT_TRUE(restored_keys.HasKeyPair(1));
}

TEST(CrossUserSharingKeysTest, ShouldReplacePreexistingKeyPair) {
  CrossUserSharingKeys keys = CrossUserSharingKeys::CreateEmpty();
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  0);
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  1);

  auto raw_existing_private_key0 =
      keys.GetKeyPair(/*version=*/0).GetRawPrivateKey();
  auto raw_existing_private_key1 =
      keys.GetKeyPair(/*version=*/1).GetRawPrivateKey();

  // Replace the existing key pair.
  keys.SetKeyPair(CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair(),
                  0);

  EXPECT_NE(raw_existing_private_key0,
            keys.GetKeyPair(/*version=*/0).GetRawPrivateKey());
  EXPECT_EQ(raw_existing_private_key1,
            keys.GetKeyPair(/*version=*/1).GetRawPrivateKey());
}

}  // namespace
}  // namespace syncer
