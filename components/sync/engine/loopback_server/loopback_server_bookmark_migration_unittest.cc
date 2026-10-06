// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/loopback_server_bookmark_migration.h"

#include <string>

#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

constexpr char kOtherBookmarksFolderUuid[] =
    "82b081ec-3dd3-529c-8475-ab6c344590dd";

TEST(LoopbackServerBookmarkMigrationTest,
     ShouldMigrateLegacyBookmarkEntitiesAndTombstones) {
  sync_pb::LoopbackServerProto proto;
  proto.set_version(1);
  proto.set_store_birthday(12345);
  proto.set_last_version_assigned(100);

  const std::string folder_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string bookmark_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string deleted_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  // 1. Permanent bookmark bar folder.
  sync_pb::LoopbackServerEntity* bar_entity = proto.add_entities();
  bar_entity->set_type(sync_pb::LoopbackServerEntity_Type_PERMANENT);
  sync_pb::SyncEntity* bar_sync = bar_entity->mutable_entity();
  bar_sync->set_id_string("32904_bookmark_bar");
  bar_sync->set_server_defined_unique_tag("bookmark_bar");
  bar_sync->set_parent_id_string("32904_google_chrome_bookmarks");
  bar_sync->set_name("Bookmark Bar");
  bar_sync->set_version(1);
  bar_sync->mutable_specifics()->mutable_bookmark();

  // 2. Legacy bookmark folder without `parent_guid` in specifics (tests
  // fallback to `kOtherBookmarksFolderUuid`).
  sync_pb::LoopbackServerEntity* folder_entity = proto.add_entities();
  folder_entity->set_type(sync_pb::LoopbackServerEntity_Type_BOOKMARK);
  sync_pb::SyncEntity* folder_sync = folder_entity->mutable_entity();
  folder_sync->set_id_string("32904_" + folder_guid);
  folder_sync->set_version(10);
  folder_sync->set_name("Folder 1");
  folder_sync->set_folder(true);
  folder_sync->set_parent_id_string("32904_bookmark_bar");
  folder_sync->set_originator_cache_guid("legacy_cache_guid");
  folder_sync->set_originator_client_item_id(folder_guid);
  folder_sync->mutable_unique_position()->set_value(
      std::string(UniquePosition::kSuffixLength, 'a'));
  folder_sync->mutable_specifics()->mutable_bookmark();

  // 3. Legacy bookmark inside the folder with `parent_guid` already populated
  // in specifics (preserved during migration).
  sync_pb::LoopbackServerEntity* bookmark_entity = proto.add_entities();
  bookmark_entity->set_type(sync_pb::LoopbackServerEntity_Type_BOOKMARK);
  sync_pb::SyncEntity* bookmark_sync = bookmark_entity->mutable_entity();
  bookmark_sync->set_id_string("32904_" + bookmark_guid);
  bookmark_sync->set_version(20);
  bookmark_sync->set_name("URL 1");
  bookmark_sync->set_folder(false);
  bookmark_sync->set_parent_id_string("32904_" + folder_guid);
  bookmark_sync->set_originator_cache_guid("legacy_cache_guid");
  bookmark_sync->set_originator_client_item_id(bookmark_guid);
  *bookmark_sync->mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  bookmark_sync->mutable_specifics()->mutable_bookmark()->set_url(
      "https://google.com/");
  bookmark_sync->mutable_specifics()->mutable_bookmark()->set_parent_guid(
      folder_guid);

  // 4. Legacy deleted tombstone entity.
  sync_pb::LoopbackServerEntity* tombstone_entity = proto.add_entities();
  tombstone_entity->set_type(sync_pb::LoopbackServerEntity_Type_TOMBSTONE);
  tombstone_entity->set_data_type(
      GetSpecificsFieldNumberFromDataType(syncer::BOOKMARKS));
  sync_pb::SyncEntity* tombstone_sync = tombstone_entity->mutable_entity();
  tombstone_sync->set_id_string("32904_" + deleted_guid);
  tombstone_sync->set_deleted(true);
  tombstone_sync->set_version(30);

  MigrateLoopbackServerLegacyBookmarks(&proto);

  // Store birthday should be bumped.
  EXPECT_NE(12345, proto.store_birthday());

  const std::string expected_folder_tag_hash =
      ClientTagHash::FromUnhashed(syncer::BOOKMARKS, folder_guid).value();
  const std::string expected_bookmark_tag_hash =
      ClientTagHash::FromUnhashed(syncer::BOOKMARKS, bookmark_guid).value();
  const std::string expected_deleted_tag_hash =
      ClientTagHash::FromUnhashed(syncer::BOOKMARKS, deleted_guid).value();

  const std::string expected_folder_id = LoopbackServerEntity::CreateId(
      syncer::BOOKMARKS, expected_folder_tag_hash, 0);
  const std::string expected_bookmark_id = LoopbackServerEntity::CreateId(
      syncer::BOOKMARKS, expected_bookmark_tag_hash, 0);
  const std::string expected_deleted_id = LoopbackServerEntity::CreateId(
      syncer::BOOKMARKS, expected_deleted_tag_hash, 0);

  // Verify migrated folder entity.
  EXPECT_EQ(sync_pb::LoopbackServerEntity_Type_UNIQUE,
            proto.entities(1).type());
  const sync_pb::SyncEntity& migrated_folder = proto.entities(1).entity();
  EXPECT_EQ(expected_folder_id, migrated_folder.id_string());
  EXPECT_EQ(expected_folder_tag_hash, migrated_folder.client_tag_hash());
  EXPECT_FALSE(migrated_folder.has_parent_id_string());
  EXPECT_FALSE(migrated_folder.has_unique_position());
  EXPECT_FALSE(migrated_folder.has_folder());
  EXPECT_TRUE(migrated_folder.originator_cache_guid().empty());
  EXPECT_TRUE(migrated_folder.originator_client_item_id().empty());
  EXPECT_EQ(folder_guid, migrated_folder.specifics().bookmark().guid());
  EXPECT_EQ(
      "Folder 1",
      migrated_folder.specifics().bookmark().legacy_canonicalized_title());
  EXPECT_EQ("Folder 1", migrated_folder.specifics().bookmark().full_title());
  EXPECT_EQ(kOtherBookmarksFolderUuid,
            migrated_folder.specifics().bookmark().parent_guid());
  EXPECT_EQ(sync_pb::BookmarkSpecifics::FOLDER,
            migrated_folder.specifics().bookmark().type());
  EXPECT_TRUE(migrated_folder.specifics().bookmark().has_unique_position());
  EXPECT_FALSE(
      migrated_folder.specifics().bookmark().unique_position().has_value());
  EXPECT_TRUE(migrated_folder.specifics()
                  .bookmark()
                  .unique_position()
                  .has_custom_compressed_v1());
  EXPECT_TRUE(UniquePosition::FromProto(
                  migrated_folder.specifics().bookmark().unique_position())
                  .IsValid());

  // Verify migrated bookmark entity.
  EXPECT_EQ(sync_pb::LoopbackServerEntity_Type_UNIQUE,
            proto.entities(2).type());
  const sync_pb::SyncEntity& migrated_bookmark = proto.entities(2).entity();
  EXPECT_EQ(expected_bookmark_id, migrated_bookmark.id_string());
  EXPECT_EQ(expected_bookmark_tag_hash, migrated_bookmark.client_tag_hash());
  EXPECT_FALSE(migrated_bookmark.has_parent_id_string());
  EXPECT_FALSE(migrated_bookmark.has_unique_position());
  EXPECT_FALSE(migrated_bookmark.has_folder());
  EXPECT_TRUE(migrated_bookmark.originator_cache_guid().empty());
  EXPECT_TRUE(migrated_bookmark.originator_client_item_id().empty());
  EXPECT_EQ(bookmark_guid, migrated_bookmark.specifics().bookmark().guid());
  EXPECT_EQ(
      "URL 1",
      migrated_bookmark.specifics().bookmark().legacy_canonicalized_title());
  EXPECT_EQ("URL 1", migrated_bookmark.specifics().bookmark().full_title());
  EXPECT_EQ(folder_guid,
            migrated_bookmark.specifics().bookmark().parent_guid());
  EXPECT_EQ(sync_pb::BookmarkSpecifics::URL,
            migrated_bookmark.specifics().bookmark().type());
  EXPECT_EQ("https://google.com/",
            migrated_bookmark.specifics().bookmark().url());
  EXPECT_TRUE(migrated_bookmark.specifics()
                  .bookmark()
                  .unique_position()
                  .has_custom_compressed_v1());

  // Verify migrated tombstone entity.
  EXPECT_EQ(sync_pb::LoopbackServerEntity_Type_TOMBSTONE,
            proto.entities(3).type());
  const sync_pb::SyncEntity& migrated_tombstone = proto.entities(3).entity();
  EXPECT_EQ(expected_deleted_id, migrated_tombstone.id_string());
  EXPECT_EQ(expected_deleted_tag_hash, migrated_tombstone.client_tag_hash());
}

TEST(LoopbackServerBookmarkMigrationTest,
     ShouldMigrateEncryptedLegacyBookmarkEntity) {
  sync_pb::LoopbackServerProto proto;
  proto.set_version(1);
  proto.set_store_birthday(12345);
  proto.set_last_version_assigned(100);

  const std::string bookmark_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::LoopbackServerEntity* bookmark_entity = proto.add_entities();
  bookmark_entity->set_type(sync_pb::LoopbackServerEntity_Type_BOOKMARK);
  sync_pb::SyncEntity* bookmark_sync = bookmark_entity->mutable_entity();
  bookmark_sync->set_id_string("32904_" + bookmark_guid);
  bookmark_sync->set_version(20);
  bookmark_sync->set_name("encrypted");
  bookmark_sync->set_folder(true);
  bookmark_sync->set_parent_id_string("32904_bookmark_bar");
  bookmark_sync->set_originator_cache_guid("legacy_cache_guid");
  bookmark_sync->set_originator_client_item_id(bookmark_guid);
  bookmark_sync->mutable_specifics()->mutable_bookmark();
  bookmark_sync->mutable_specifics()->mutable_encrypted()->set_key_name("key1");
  bookmark_sync->mutable_specifics()->mutable_encrypted()->set_blob(
      "ciphertext");

  MigrateLoopbackServerLegacyBookmarks(&proto);

  const std::string expected_tag_hash =
      ClientTagHash::FromUnhashed(syncer::BOOKMARKS, bookmark_guid).value();
  const std::string expected_id =
      LoopbackServerEntity::CreateId(syncer::BOOKMARKS, expected_tag_hash, 0);

  EXPECT_EQ(sync_pb::LoopbackServerEntity_Type_UNIQUE,
            proto.entities(0).type());
  const sync_pb::SyncEntity& migrated_bookmark = proto.entities(0).entity();
  EXPECT_EQ(expected_id, migrated_bookmark.id_string());
  EXPECT_EQ(expected_tag_hash, migrated_bookmark.client_tag_hash());
  EXPECT_FALSE(migrated_bookmark.has_folder());
  EXPECT_TRUE(migrated_bookmark.originator_cache_guid().empty());
  EXPECT_TRUE(migrated_bookmark.originator_client_item_id().empty());
  EXPECT_EQ("ciphertext", migrated_bookmark.specifics().encrypted().blob());
  EXPECT_FALSE(migrated_bookmark.specifics().bookmark().has_guid());
  EXPECT_FALSE(migrated_bookmark.specifics().bookmark().has_parent_guid());
  EXPECT_FALSE(migrated_bookmark.specifics().bookmark().has_type());
}

}  // namespace

}  // namespace syncer
