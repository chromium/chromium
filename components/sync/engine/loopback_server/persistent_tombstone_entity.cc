// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

using std::string;

using syncer::DataType;

namespace syncer {

PersistentTombstoneEntity::~PersistentTombstoneEntity() = default;

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateFromEntity(const sync_pb::SyncEntity& entity) {
  return CreateNewInternal(entity.id_string(), entity.version(),
                           entity.client_tag_hash());
}

// static
std::unique_ptr<LoopbackServerEntity> PersistentTombstoneEntity::CreateNew(
    const std::string& id,
    const std::string& client_tag_hash) {
  return CreateNewInternal(id, 0, client_tag_hash);
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentTombstoneEntity::CreateNewInternal(
    const std::string& id,
    int64_t version,
    const std::string& client_tag_hash) {
  const DataType data_type = LoopbackServerEntity::GetDataTypeFromId(id);
  if (data_type == syncer::UNSPECIFIED) {
    DLOG(WARNING) << "Invalid ID was given: " << id;
    return nullptr;
  }

  return base::WrapUnique(
      new PersistentTombstoneEntity(id, version, data_type, client_tag_hash));
}

PersistentTombstoneEntity::PersistentTombstoneEntity(
    const string& id,
    int64_t version,
    const DataType& data_type,
    const std::string& client_tag_hash)
    : LoopbackServerEntity(id, data_type, version, string()),
      client_tag_hash_(client_tag_hash) {
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
  if (!client_tag_hash_.empty()) {
    proto->set_client_tag_hash(client_tag_hash_);
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
