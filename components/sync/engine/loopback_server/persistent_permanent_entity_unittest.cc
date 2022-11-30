// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_permanent_entity.h"

#include "components/sync/protocol/sync_entity.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentPermanentEntityTest, CreateNew) {
  ASSERT_FALSE(PersistentPermanentEntity::CreateNew(
      UNSPECIFIED, "server_tag", "name", "parent_server_tag"));
  ASSERT_FALSE(PersistentPermanentEntity::CreateNew(PREFERENCES, "", "name",
                                                    "parent_server_tag"));
  ASSERT_FALSE(PersistentPermanentEntity::CreateNew(PREFERENCES, "server_tag",
                                                    "", "parent_server_tag"));
  ASSERT_FALSE(PersistentPermanentEntity::CreateNew(PREFERENCES, "server_tag",
                                                    "name", ""));
  ASSERT_FALSE(PersistentPermanentEntity::CreateNew(PREFERENCES, "server_tag",
                                                    "name", "0"));
  ASSERT_TRUE(PersistentPermanentEntity::CreateNew(
      PREFERENCES, "server_tag", "name", "parent_server_tag"));
}

TEST(PersistentPermanentEntityTest, CreateTopLevel) {
  ASSERT_FALSE(PersistentPermanentEntity::CreateTopLevel(UNSPECIFIED));
  ASSERT_TRUE(PersistentPermanentEntity::CreateTopLevel(PREFERENCES));
}

TEST(PersistentPermanentEntityTest, CreateUpdatedNigoriEntity) {
  sync_pb::SyncEntity client_entity;
  client_entity.mutable_specifics()->mutable_nigori();

  auto preferences_server_entity = PersistentPermanentEntity::CreateNew(
      PREFERENCES, "server_tag", "name", "parent_server_tag");
  ASSERT_TRUE(preferences_server_entity);
  ASSERT_FALSE(PersistentPermanentEntity::CreateUpdatedNigoriEntity(
      client_entity, *preferences_server_entity));

  auto nigori_server_entity = PersistentPermanentEntity::CreateNew(
      NIGORI, "server_tag", "name", "parent_server_tag");
  ASSERT_TRUE(nigori_server_entity);
  ASSERT_TRUE(PersistentPermanentEntity::CreateUpdatedNigoriEntity(
      client_entity, *nigori_server_entity));
}

}  // namespace

}  // namespace syncer
