// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_bookmark_entity.h"

#include <memory>

#include "base/uuid.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentBookmarkEntityTest, CreateNew) {
  sync_pb::SyncEntity entity;
  entity.set_id_string(base::Uuid::GenerateRandomV4().AsLowercaseString());

  entity.mutable_specifics()->mutable_preference();
  EXPECT_FALSE(PersistentBookmarkEntity::CreateNew(entity, "parent_id",
                                                   "client_guid",
                                                   /*migration_version=*/0));

  entity.clear_specifics();
  entity.mutable_specifics()->mutable_bookmark();
  EXPECT_TRUE(PersistentBookmarkEntity::CreateNew(entity, "parent_id",
                                                  "client_guid",
                                                  /*migration_version=*/0));
}

TEST(PersistentBookmarkEntityTest, CreateUpdatedVersion) {
  sync_pb::SyncEntity client_entity;
  client_entity.set_id_string(
      "32904_" + base::Uuid::GenerateRandomV4().AsLowercaseString());
  client_entity.mutable_specifics()->mutable_bookmark();
  std::unique_ptr<PersistentBookmarkEntity> server_entity =
      PersistentBookmarkEntity::CreateFromEntity(client_entity);
  ASSERT_TRUE(server_entity);
  EXPECT_EQ(server_entity->AsBookmarkEntity(), server_entity.get());

  // Fails since there's no version.
  ASSERT_FALSE(PersistentBookmarkEntity::CreateUpdatedVersion(
      client_entity, *server_entity, "parent_id", "updating_guid"));

  // And now succeeds that we have a version.
  client_entity.set_version(1);
  ASSERT_TRUE(PersistentBookmarkEntity::CreateUpdatedVersion(
      client_entity, *server_entity, "parent_id", "updating_guid"));

  // Succeeds when the existing server entity is not a bookmark entity (e.g. a
  // tombstone or unexpected non-bookmark entity), replacing it using the
  // committing client's metadata without downcasting.
  sync_pb::EntitySpecifics unique_specifics;
  unique_specifics.mutable_preference();
  std::unique_ptr<LoopbackServerEntity> non_bookmark_server_entity =
      PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", "client_tag", unique_specifics,
          /*creation_time=*/0, /*last_modified_time=*/0);
  ASSERT_TRUE(non_bookmark_server_entity);
  EXPECT_EQ(non_bookmark_server_entity->AsBookmarkEntity(), nullptr);
  std::unique_ptr<PersistentBookmarkEntity> replaced_entity =
      PersistentBookmarkEntity::CreateUpdatedVersion(
          client_entity, *non_bookmark_server_entity, "parent_id",
          "updating_guid");
  ASSERT_TRUE(replaced_entity);
  sync_pb::SyncEntity replaced_proto;
  replaced_entity->SerializeAsProto(&replaced_proto);
  EXPECT_EQ(replaced_proto.originator_cache_guid(), "updating_guid");

  // Fails when client_entity is not actually a bookmark.
  client_entity.clear_specifics();
  client_entity.mutable_specifics()->mutable_preference();
  ASSERT_FALSE(PersistentBookmarkEntity::CreateUpdatedVersion(
      client_entity, *server_entity, "parent_id", "updating_guid"));
}

TEST(PersistentBookmarkEntityTest, CreateFromEntity) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_preference();
  EXPECT_FALSE(PersistentBookmarkEntity::CreateFromEntity(entity));

  entity.clear_specifics();
  entity.mutable_specifics()->mutable_bookmark();
  EXPECT_TRUE(PersistentBookmarkEntity::CreateFromEntity(entity));
}

}  // namespace

}  // namespace syncer
