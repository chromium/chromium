// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentUniqueClientEntityTest, CreateFromEntity) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_preference();
  // Normal types need a client_defined_unique_tag.
  ASSERT_FALSE(PersistentUniqueClientEntity::CreateFromEntity(entity));

  *entity.mutable_client_defined_unique_tag() = "tag";
  ASSERT_TRUE(PersistentUniqueClientEntity::CreateFromEntity(entity));

  entity.clear_specifics();
  entity.mutable_specifics()->mutable_user_event();
  // CommitOnly type should never have a client_defined_unique_tag.
  ASSERT_FALSE(PersistentUniqueClientEntity::CreateFromEntity(entity));

  entity.clear_client_defined_unique_tag();
  ASSERT_TRUE(PersistentUniqueClientEntity::CreateFromEntity(entity));
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
  EXPECT_EQ(syncer::PREFERENCES, entity->GetModelType());
  EXPECT_EQ(
      LoopbackServerEntity::CreateId(
          syncer::PREFERENCES,
          ClientTagHash::FromUnhashed(syncer::PREFERENCES, kClientTag).value()),
      entity->GetId());
}

}  // namespace

}  // namespace syncer
