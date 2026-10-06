// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/loopback_server_bookmark_migration.h"

#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

namespace {

constexpr char kOtherBookmarksFolderUuid[] =
    "82b081ec-3dd3-529c-8475-ab6c344590dd";

base::Uuid DetermineBookmarkUuid(const sync_pb::SyncEntity& entity) {
  base::Uuid uuid =
      base::Uuid::ParseLowercase(entity.specifics().bookmark().guid());
  if (uuid.is_valid()) {
    return uuid;
  }
  // `LoopbackServer` was introduced in M57+, after `originator_client_item_id`
  // was standardized to be a lowercase UUID in M52.
  uuid = base::Uuid::ParseCaseInsensitive(entity.originator_client_item_id());
  if (uuid.is_valid()) {
    return uuid;
  }
  // Fallback case, although expected to be reachable only upon data corruption.
  return base::Uuid::GenerateRandomV4();
}

UniquePosition DetermineUniquePosition(const sync_pb::SyncEntity& entity) {
  UniquePosition pos = UniquePosition::FromProto(
      entity.specifics().bookmark().unique_position());
  if (!pos.IsValid()) {
    pos = UniquePosition::FromProto(entity.unique_position());
  }
  if (!pos.IsValid()) {
    pos = UniquePosition::InitialPosition(UniquePosition::RandomSuffix());
  }
  return pos;
}

void PopulateModernBookmarkSpecifics(const base::Uuid& uuid,
                                     sync_pb::SyncEntity* entity) {
  // For encrypted entities, BookmarkSpecifics are opaque inside
  // `specifics().encrypted()` and are assumed to already contain modern
  // fields. Do not populate unencrypted fields in `specifics().bookmark()`.
  if (entity->specifics().has_encrypted()) {
    return;
  }

  sync_pb::BookmarkSpecifics* bookmark =
      entity->mutable_specifics()->mutable_bookmark();
  // Infer folderness from the presence of `url` in `BookmarkSpecifics`
  // (populated if and only if the bookmark is a URL). This avoids relying on
  // `SyncEntity.folder` and guarantees `type` is never `UNSPECIFIED` or set to
  // `URL` without a `url` field (which `IsValidBookmarkSpecifics()` rejects).
  bookmark->set_type(bookmark->has_url() ? sync_pb::BookmarkSpecifics::URL
                                         : sync_pb::BookmarkSpecifics::FOLDER);
  if (!bookmark->has_legacy_canonicalized_title()) {
    bookmark->set_legacy_canonicalized_title(
        bookmark->has_full_title() ? bookmark->full_title() : entity->name());
  }
  if (!bookmark->has_full_title()) {
    bookmark->set_full_title(bookmark->legacy_canonicalized_title());
  }
  *bookmark->mutable_unique_position() =
      DetermineUniquePosition(*entity).ToProto();
  if (!base::Uuid::ParseLowercase(bookmark->parent_guid()).is_valid()) {
    // Fallback mechanism that shouldn't be exercised in normal circumstances,
    // as all bookmarks should have already been reuploaded with `parent_guid`
    // populated in specifics.
    bookmark->set_parent_guid(kOtherBookmarksFolderUuid);
  }
  bookmark->set_guid(uuid.AsLowercaseString());
}

void MigrateBookmarkEntity(sync_pb::LoopbackServerEntity* server_entity) {
  sync_pb::SyncEntity* entity = server_entity->mutable_entity();
  const base::Uuid uuid = DetermineBookmarkUuid(*entity);
  entity->set_client_tag_hash(
      ClientTagHash::FromUnhashed(syncer::BOOKMARKS, uuid.AsLowercaseString())
          .value());
  PopulateModernBookmarkSpecifics(uuid, entity);

  entity->set_id_string(LoopbackServerEntity::CreateId(
      syncer::BOOKMARKS, entity->client_tag_hash(),
      /*migration_version=*/0));
  entity->clear_parent_id_string();
  entity->clear_unique_position();
  entity->clear_folder();
  entity->clear_originator_cache_guid();
  entity->clear_originator_client_item_id();

  server_entity->set_type(sync_pb::LoopbackServerEntity_Type_UNIQUE);
}

void MigrateBookmarkTombstoneIfApplicable(
    sync_pb::LoopbackServerEntity* server_entity) {
  sync_pb::SyncEntity* entity = server_entity->mutable_entity();
  const DataType data_type =
      server_entity->has_data_type()
          ? syncer::GetDataTypeFromSpecificsFieldNumber(
                server_entity->data_type())
          : LoopbackServerEntity::GetDataTypeFromId(entity->id_string());
  if (data_type != syncer::BOOKMARKS) {
    return;
  }

  const std::string inner_id =
      LoopbackServerEntity::GetInnerIdFromId(entity->id_string());
  std::string client_tag_hash;
  if (!entity->client_tag_hash().empty()) {
    client_tag_hash = entity->client_tag_hash();
  } else if (const base::Uuid uuid = base::Uuid::ParseCaseInsensitive(inner_id);
             uuid.is_valid()) {
    client_tag_hash =
        ClientTagHash::FromUnhashed(syncer::BOOKMARKS, uuid.AsLowercaseString())
            .value();
  } else {
    // Unexpected corrupt tombstone ID; leave the entity untouched.
    return;
  }

  entity->set_client_tag_hash(client_tag_hash);
  entity->set_id_string(LoopbackServerEntity::CreateId(
      syncer::BOOKMARKS, client_tag_hash, /*migration_version=*/0));
}

}  // namespace

void MigrateLoopbackServerLegacyBookmarks(sync_pb::LoopbackServerProto* proto) {
  CHECK(proto);

  for (sync_pb::LoopbackServerEntity& server_entity :
       *proto->mutable_entities()) {
    switch (server_entity.type()) {
      case sync_pb::LoopbackServerEntity_Type_BOOKMARK:
        MigrateBookmarkEntity(&server_entity);
        break;
      case sync_pb::LoopbackServerEntity_Type_TOMBSTONE:
        MigrateBookmarkTombstoneIfApplicable(&server_entity);
        break;
      case sync_pb::LoopbackServerEntity_Type_UNKNOWN:
      case sync_pb::LoopbackServerEntity_Type_PERMANENT:
      case sync_pb::LoopbackServerEntity_Type_UNIQUE:
        break;
    }
  }

  // Bump the store birthday so that any existing syncing clients receive
  // NOT_MY_BIRTHDAY, reset their local sync state, and download the migrated
  // entities cleanly.
  int64_t new_birthday = base::Time::Now().InMillisecondsSinceUnixEpoch();
  if (new_birthday == proto->store_birthday()) {
    new_birthday++;
  }
  proto->set_store_birthday(new_birthday);
}

}  // namespace syncer
