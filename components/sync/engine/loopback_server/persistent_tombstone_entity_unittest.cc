// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"

#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentTombstoneEntityTest, CreateFromEntity) {
  sync_pb::SyncEntity entity;
  *entity.mutable_id_string() = "invalid_id";
  ASSERT_FALSE(PersistentTombstoneEntity::CreateFromEntity(entity));
  *entity.mutable_id_string() = "37702_id";
  ASSERT_TRUE(PersistentTombstoneEntity::CreateFromEntity(entity));
}

TEST(PersistentTombstoneEntityTest, CreateNew) {
  ASSERT_FALSE(PersistentTombstoneEntity::CreateNewForTest(
      "invalid_id", ClientTagHash::FromHashed("client_tag_hash")));
  ASSERT_TRUE(PersistentTombstoneEntity::CreateNewForTest(
      "37702_id", ClientTagHash::FromHashed("client_tag_hash")));
}

}  // namespace

}  // namespace syncer
