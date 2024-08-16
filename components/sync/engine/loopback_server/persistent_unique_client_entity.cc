// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/loopback_server/persistent_permanent_entity.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

PersistentUniqueClientEntity::PersistentUniqueClientEntity(
    const std::string& id,
    DataType data_type,
    int64_t version,
    const std::string& name,
    const std::string& client_tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t creation_time,
    int64_t last_modified_time,
    const std::string& collaboration_id)
    : LoopbackServerEntity(id, data_type, version, name),
      client_tag_hash_(client_tag_hash),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time),
      collaboration_id_(collaboration_id) {
  SetSpecifics(specifics);
}

PersistentUniqueClientEntity::~PersistentUniqueClientEntity() = default;

// static
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromEntity(
    const sync_pb::SyncEntity& client_entity) {
  DataType data_type = GetDataTypeFromSpecifics(client_entity.specifics());
  if (!client_entity.has_client_tag_hash()) {
    DLOG(WARNING) << "A UniqueClientEntity should have a client-defined unique "
                     "tag.";
    return nullptr;
  }

  // Without data type specific logic for each CommitOnly type, we cannot infer
  // a reasonable tag from the specifics. We need uniqueness for how the server
  // holds onto all objects, so simply make a new tag from a random  number.
  std::string effective_tag = client_entity.has_client_tag_hash()
                                  ? client_entity.client_tag_hash()
                                  : base::NumberToString(base::RandUint64());
  std::string id = LoopbackServerEntity::CreateId(data_type, effective_tag);
  return std::make_unique<PersistentUniqueClientEntity>(
      id, data_type, client_entity.version(), client_entity.name(),
      client_entity.client_tag_hash(), client_entity.specifics(),
      client_entity.ctime(), client_entity.mtime(),
      client_entity.collaboration().collaboration_id());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
    const std::string& non_unique_name,
    const std::string& client_tag,
    const sync_pb::EntitySpecifics& entity_specifics,
    int64_t creation_time,
    int64_t last_modified_time) {
  DataType data_type = GetDataTypeFromSpecifics(entity_specifics);
  std::string client_tag_hash =
      ClientTagHash::FromUnhashed(data_type, client_tag).value();
  std::string id = LoopbackServerEntity::CreateId(data_type, client_tag_hash);
  return std::make_unique<PersistentUniqueClientEntity>(
      id, data_type, 0, non_unique_name, client_tag_hash, entity_specifics,
      creation_time, last_modified_time, /*collaboration_id=*/"");
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromSharedSpecificsForTesting(
    const std::string& non_unique_name,
    const std::string& client_tag,
    const sync_pb::EntitySpecifics& entity_specifics,
    int64_t creation_time,
    int64_t last_modified_time,
    const std::string& collaboration_id) {
  DataType data_type = GetDataTypeFromSpecifics(entity_specifics);
  std::string client_tag_hash =
      ClientTagHash::FromUnhashed(data_type, client_tag).value();
  std::string id = LoopbackServerEntity::CreateId(data_type, client_tag_hash);
  return std::make_unique<PersistentUniqueClientEntity>(
      id, data_type, 0, non_unique_name, client_tag_hash, entity_specifics,
      creation_time, last_modified_time, collaboration_id);
}

bool PersistentUniqueClientEntity::RequiresParentId() const {
  return false;
}

std::string PersistentUniqueClientEntity::GetParentId() const {
  // The parent ID for this type of entity should always be its DataType's
  // root node.
  return LoopbackServerEntity::GetTopLevelId(GetDataType());
}

sync_pb::LoopbackServerEntity_Type
PersistentUniqueClientEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_UNIQUE;
}

void PersistentUniqueClientEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);

  proto->set_client_tag_hash(client_tag_hash_);
  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);
  if (!collaboration_id_.empty()) {
    proto->mutable_collaboration()->set_collaboration_id(collaboration_id_);
  }
}

}  // namespace syncer
