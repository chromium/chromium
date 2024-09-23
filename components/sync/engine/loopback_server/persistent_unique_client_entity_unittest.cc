// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentUniqueClientEntityTest, CreateFromEntity) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_preference();
  // Normal types need a client_tag_hash.
  ASSERT_FALSE(PersistentUniqueClientEntity::CreateFromEntity(entity));

  *entity.mutable_client_tag_hash() = "tag";
  ASSERT_TRUE(PersistentUniqueClientEntity::CreateFromEntity(entity));

  entity.clear_specifics();
  entity.mutable_specifics()->mutable_user_event();
  // CommitOnly type should also have a client_tag_hash.
  ASSERT_TRUE(PersistentUniqueClientEntity::CreateFromEntity(entity));

  entity.clear_client_tag_hash();
  ASSERT_FALSE(PersistentUniqueClientEntity::CreateFromEntity(entity));
}

TEST(PersistentUniqueClientEntityTest, CreateFromSpecificsForTesting) {
  const std::string kNonUniqueName = "somename";
  const std::string kClientTag = "someclienttag";

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference();

  std::unique_ptr<LoopbackServerEntity> entity =
      PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          kNonUniqueName, kClientTag, specifics, 0, 0);

  ASSERT_TRUE(entity);
  EXPECT_EQ(kNonUniqueName, entity->GetName());
  EXPECT_EQ(syncer::PREFERENCES, entity->GetDataType());
  EXPECT_EQ(
      LoopbackServerEntity::CreateId(
          syncer::PREFERENCES,
          ClientTagHash::FromUnhashed(syncer::PREFERENCES, kClientTag).value()),
      entity->GetId());
}

}  // namespace

}  // namespace syncer
