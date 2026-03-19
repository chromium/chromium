// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

using std::string;

using syncer::DataType;

namespace syncer {

namespace {

std::string CreateServerId(syncer::DataType data_type,
                           std::string_view client_tag) {
  // For all data types except bookmarks, the server ID is built based on the
  // client tag *hash*. For bookmarks, the non-hashed client tag (aka UUID) is
  // used.
  if (data_type == BOOKMARKS) {
    return LoopbackServerEntity::CreateId(data_type, std::string(client_tag));
  }

  return LoopbackServerEntity::CreateId(
      data_type, ClientTagHash::FromUnhashed(data_type, client_tag).value());
}

}  // namespace

PersistentTombstoneEntity::~PersistentTombstoneEntity() = default;

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateFromEntity(const sync_pb::SyncEntity& entity) {
  return CreateNewInternal(entity.id_string(), entity.version(),
                           ClientTagHash::FromHashed(entity.client_tag_hash()),
                           entity.collaboration());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateNewForTest(
    const std::string& id,
    const ClientTagHash& client_tag_hash) {
  return CreateNewInternal(id, /*version=*/0, client_tag_hash,
                           sync_pb::SyncEntity::CollaborationMetadata());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateNewForTest(syncer::DataType data_type,
                                            std::string_view client_tag) {
  return CreateNewInternal(CreateServerId(data_type, client_tag),
                           /*version=*/0,
                           ClientTagHash::FromUnhashed(data_type, client_tag),
                           sync_pb::SyncEntity::CollaborationMetadata());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateNewSharedForTest(
    syncer::DataType data_type,
    std::string_view client_tag,
    const sync_pb::SyncEntity::CollaborationMetadata& collaboration_metadata) {
  return CreateNewInternal(CreateServerId(data_type, client_tag),
                           /*version=*/0,
                           ClientTagHash::FromUnhashed(data_type, client_tag),
                           collaboration_metadata);
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateNewInternal(
    const std::string& id,
    int64_t version,
    const ClientTagHash& client_tag_hash,
    const sync_pb::SyncEntity::CollaborationMetadata& collaboration_metadata) {
  const DataType data_type = LoopbackServerEntity::GetDataTypeFromId(id);
  if (data_type == syncer::UNSPECIFIED) {
    DLOG(WARNING) << "Invalid ID was given: " << id;
    return nullptr;
  }

  return base::WrapUnique(new PersistentTombstoneEntity(
      id, version, data_type, client_tag_hash, collaboration_metadata));
}

PersistentTombstoneEntity::PersistentTombstoneEntity(
    const string& id,
    int64_t version,
    const DataType& data_type,
    const ClientTagHash& client_tag_hash,
    const sync_pb::SyncEntity::CollaborationMetadata& collaboration_metadata)
    : LoopbackServerEntity(id, data_type, version, string()),
      client_tag_hash_(client_tag_hash),
      collaboration_metadata_(collaboration_metadata) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(data_type, &specifics);
  SetSpecifics(specifics);
}

bool PersistentTombstoneEntity::RequiresParentId() const {
  return false;
}

string PersistentTombstoneEntity::GetParentId() const {
  return string();
}

void PersistentTombstoneEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);
  if (!client_tag_hash_.value().empty()) {
    proto->set_client_tag_hash(client_tag_hash_.value());
  }
  if (collaboration_metadata_.has_collaboration_id()) {
    *proto->mutable_collaboration() = collaboration_metadata_;
  }
}

bool PersistentTombstoneEntity::IsDeleted() const {
  return true;
}

sync_pb::LoopbackServerEntity_Type
PersistentTombstoneEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_TOMBSTONE;
}

}  // namespace syncer
