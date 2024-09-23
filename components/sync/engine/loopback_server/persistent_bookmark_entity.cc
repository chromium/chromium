// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_bookmark_entity.h"

#include <memory>

#include "base/logging.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

using std::string;

namespace syncer {

namespace {

// Returns true if and only if |client_entity| is a bookmark.
bool IsBookmark(const sync_pb::SyncEntity& client_entity) {
  return syncer::GetDataTypeFromSpecifics(client_entity.specifics()) ==
         syncer::BOOKMARKS;
}

}  // namespace

PersistentBookmarkEntity::~PersistentBookmarkEntity() = default;

// static
std::unique_ptr<LoopbackServerEntity> PersistentBookmarkEntity::CreateNew(
    const sync_pb::SyncEntity& client_entity,
    const string& parent_id,
    const string& originator_cache_guid) {
  if (!IsBookmark(client_entity)) {
    DLOG(WARNING) << "The given entity must be a bookmark.";
    return nullptr;
  }

  const std::string originator_client_item_id = client_entity.id_string();
  if (!base::Uuid::ParseLowercase(originator_client_item_id).is_valid()) {
    DLOG(WARNING) << "Invalid originator client item ID: "
                  << originator_client_item_id;
    return nullptr;
  }

  const string id = LoopbackServerEntity::CreateId(syncer::BOOKMARKS,
                                                   originator_client_item_id);

  return std::make_unique<PersistentBookmarkEntity>(
      id, 0, client_entity.name(), originator_cache_guid,
      originator_client_item_id, client_entity.client_tag_hash(),
      client_entity.unique_position(), client_entity.specifics(),
      client_entity.folder(), parent_id, client_entity.ctime(),
      client_entity.mtime());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentBookmarkEntity::CreateUpdatedVersion(
    const sync_pb::SyncEntity& client_entity,
    const LoopbackServerEntity& current_server_entity,
    const string& parent_id,
    const std::string& updating_client_cache_guid) {
  if (client_entity.version() == 0) {
    DLOG(WARNING) << "Existing entities must not have a version = 0.";
    return nullptr;
  }

  if (!IsBookmark(client_entity)) {
    DLOG(WARNING) << "The given entity must be a bookmark.";
    return nullptr;
  }

  std::string originator_cache_guid;
  std::string originator_client_item_id;
  std::string client_tag_hash;

  if (current_server_entity.IsDeleted()) {
    // Handle undeletions.
    originator_cache_guid = updating_client_cache_guid;
    originator_client_item_id =
        LoopbackServerEntity::GetInnerIdFromId(client_entity.id_string());
    // An undeletion is similar to a creation, so let's honor the client tag
    // provided by the client (if any).
    client_tag_hash = client_entity.client_tag_hash();
  } else {
    // Regular case (non-undeletion).
    const PersistentBookmarkEntity& current_bookmark_entity =
        static_cast<const PersistentBookmarkEntity&>(current_server_entity);
    originator_cache_guid = current_bookmark_entity.originator_cache_guid_;
    originator_client_item_id =
        current_bookmark_entity.originator_client_item_id_;
    // Note that the client tag provided by the client in |client_entity| is
    // ignored during non-creations updates, since it's meant to be immutable.
    client_tag_hash = current_bookmark_entity.client_tag_hash_;
  }

  // Using a version of 0 is okay here as it'll be updated before this entity is
  // actually saved.
  return std::make_unique<PersistentBookmarkEntity>(
      client_entity.id_string(), 0, client_entity.name(), originator_cache_guid,
      originator_client_item_id, client_tag_hash,
      client_entity.unique_position(), client_entity.specifics(),
      client_entity.folder(), parent_id, client_entity.ctime(),
      client_entity.mtime());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentBookmarkEntity::CreateFromEntity(
    const sync_pb::SyncEntity& client_entity) {
  if (!IsBookmark(client_entity)) {
    DLOG(WARNING) << "The given entity must be a bookmark.";
    return nullptr;
  }

  return std::make_unique<PersistentBookmarkEntity>(
      client_entity.id_string(), client_entity.version(), client_entity.name(),
      client_entity.originator_cache_guid(),
      client_entity.originator_client_item_id(),
      client_entity.client_tag_hash(), client_entity.unique_position(),
      client_entity.specifics(), client_entity.folder(),
      client_entity.parent_id_string(), client_entity.ctime(),
      client_entity.mtime());
}
PersistentBookmarkEntity::PersistentBookmarkEntity(
    const string& id,
    int64_t version,
    const string& name,
    const string& originator_cache_guid,
    const string& originator_client_item_id,
    const string& client_tag_hash,
    const sync_pb::UniquePosition& unique_position,
    const sync_pb::EntitySpecifics& specifics,
    bool is_folder,
    const string& parent_id,
    int64_t creation_time,
    int64_t last_modified_time)
    : LoopbackServerEntity(id, syncer::BOOKMARKS, version, name),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id),
      client_tag_hash_(client_tag_hash),
      is_folder_(is_folder),
      unique_position_(unique_position),
      parent_id_(parent_id),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {
  if (!client_tag_hash.empty()) {
    // This relies technically on a well-behaving client, but verifying here to
    // avoid issues with Local Sync, which uses LoopbackServer.
    DCHECK_EQ(
        ClientTagHash::FromHashed(client_tag_hash),
        ClientTagHash::FromUnhashed(BOOKMARKS, originator_client_item_id));
  }

  SetSpecifics(specifics);
}

void PersistentBookmarkEntity::SetParentId(const string& parent_id) {
  parent_id_ = parent_id;
}

bool PersistentBookmarkEntity::RequiresParentId() const {
  // Bookmarks are stored as a hierarchy. All bookmarks must have a parent ID.
  return true;
}

string PersistentBookmarkEntity::GetParentId() const {
  return parent_id_;
}

sync_pb::LoopbackServerEntity_Type
PersistentBookmarkEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_BOOKMARK;
}

void PersistentBookmarkEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);

  if (!client_tag_hash_.empty()) {
    proto->set_client_tag_hash(client_tag_hash_);
  }

  proto->set_originator_cache_guid(originator_cache_guid_);
  proto->set_originator_client_item_id(originator_client_item_id_);

  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);

  sync_pb::UniquePosition* unique_position = proto->mutable_unique_position();
  unique_position->CopyFrom(unique_position_);
}

bool PersistentBookmarkEntity::IsFolder() const {
  return is_folder_;
}

}  // namespace syncer
