// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine_impl/loopback_server/persistent_permanent_entity.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

PersistentUniqueClientEntity::PersistentUniqueClientEntity(
    const std::string& id,
    ModelType model_type,
    int64_t version,
    const std::string& name,
    const std::string& client_tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t creation_time,
    int64_t last_modified_time)
    : LoopbackServerEntity(id, model_type, version, name),
      client_tag_hash_(client_tag_hash),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {
  SetSpecifics(specifics);
}

PersistentUniqueClientEntity::~PersistentUniqueClientEntity() {}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromEntity(
    const sync_pb::SyncEntity& client_entity) {
  ModelType model_type = GetModelTypeFromSpecifics(client_entity.specifics());
  if (client_entity.has_client_defined_unique_tag() ==
      syncer::CommitOnlyTypes().Has(model_type)) {
    DLOG(WARNING) << "A UniqueClientEntity should have a client-defined unique "
                     "tag iff it is not a CommitOnly type.";
    return nullptr;
  }

  // Without model type specific logic for each CommitOnly type, we cannot infer
  // a reasonable tag from the specifics. We need uniqueness for how the server
  // holds onto all objects, so simply make a new tag from a random  number.
  std::string effective_tag = client_entity.has_client_defined_unique_tag()
                                  ? client_entity.client_defined_unique_tag()
                                  : base::NumberToString(base::RandUint64());
  std::string id = LoopbackServerEntity::CreateId(model_type, effective_tag);
  return std::make_unique<PersistentUniqueClientEntity>(
      id, model_type, client_entity.version(), client_entity.name(),
      client_entity.client_defined_unique_tag(), client_entity.specifics(),
      client_entity.ctime(), client_entity.mtime());
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
    const std::string& non_unique_name,
    const std::string& client_tag,
    const sync_pb::EntitySpecifics& entity_specifics,
    int64_t creation_time,
    int64_t last_modified_time) {
  ModelType model_type = GetModelTypeFromSpecifics(entity_specifics);
  std::string client_tag_hash =
      ClientTagHash::FromUnhashed(model_type, client_tag).value();
  std::string id = LoopbackServerEntity::CreateId(model_type, client_tag_hash);
  return std::make_unique<PersistentUniqueClientEntity>(
      id, model_type, 0, non_unique_name, client_tag_hash, entity_specifics,
      creation_time, last_modified_time);
}

bool PersistentUniqueClientEntity::RequiresParentId() const {
  return false;
}

std::string PersistentUniqueClientEntity::GetParentId() const {
  // The parent ID for this type of entity should always be its ModelType's
  // root node.
  return LoopbackServerEntity::GetTopLevelId(GetModelType());
}

sync_pb::LoopbackServerEntity_Type
PersistentUniqueClientEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_UNIQUE;
}

void PersistentUniqueClientEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);

  proto->set_client_defined_unique_tag(client_tag_hash_);
  proto->set_ctime(creation_time_);
  proto->set_mtime(last_modified_time_);
}

}  // namespace syncer
