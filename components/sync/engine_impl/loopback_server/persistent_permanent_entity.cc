// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_permanent_entity.h"

#include <memory>

#include "base/logging.h"

using std::string;

using syncer::ModelType;

namespace {

// The parent tag for children of the root entity. Entities with this parent are
// referred to as top level enities.
static const char kRootParentTag[] = "0";

}  // namespace

namespace syncer {

PersistentPermanentEntity::~PersistentPermanentEntity() {}

// static
std::unique_ptr<LoopbackServerEntity> PersistentPermanentEntity::CreateNew(
    const ModelType& model_type,
    const string& server_tag,
    const string& name,
    const string& parent_server_tag) {
  if (model_type == syncer::UNSPECIFIED) {
    DLOG(WARNING) << "The entity's ModelType is invalid.";
    return nullptr;
  }
  if (server_tag.empty()) {
    DLOG(WARNING) << "A PersistentPermanentEntity must have a server tag.";
    return nullptr;
  }
  if (name.empty()) {
    DLOG(WARNING) << "The entity must have a non-empty name.";
    return nullptr;
  }
  if (parent_server_tag.empty()) {
    DLOG(WARNING)
        << "A PersistentPermanentEntity must have a parent server tag.";
    return nullptr;
  }
  if (parent_server_tag == kRootParentTag) {
    DLOG(WARNING)
        << "Top-level entities should not be created with this factory.";
    return nullptr;
  }

  string id = LoopbackServerEntity::CreateId(model_type, server_tag);
  string parent_id =
      LoopbackServerEntity::CreateId(model_type, parent_server_tag);
  sync_pb::EntitySpecifics entity_specifics;
  AddDefaultFieldValue(model_type, &entity_specifics);
  return std::make_unique<PersistentPermanentEntity>(
      id, 0, model_type, name, parent_id, server_tag, entity_specifics);
}

// static
std::unique_ptr<LoopbackServerEntity> PersistentPermanentEntity::CreateTopLevel(
    const ModelType& model_type) {
  if (model_type == syncer::UNSPECIFIED) {
    DLOG(WARNING) << "The entity's ModelType is invalid.";
    return nullptr;
  }

  string server_tag = syncer::ModelTypeToRootTag(model_type);
  string name = syncer::ModelTypeToString(model_type);
  string id = LoopbackServerEntity::GetTopLevelId(model_type);
  sync_pb::EntitySpecifics entity_specifics;
  AddDefaultFieldValue(model_type, &entity_specifics);
  return std::make_unique<PersistentPermanentEntity>(
      id, 0, model_type, name, kRootParentTag, server_tag, entity_specifics);
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentPermanentEntity::CreateUpdatedNigoriEntity(
    const sync_pb::SyncEntity& client_entity,
    const LoopbackServerEntity& current_server_entity) {
  ModelType model_type = current_server_entity.GetModelType();
  if (model_type != syncer::NIGORI) {
    DLOG(WARNING) << "This factory only supports NIGORI entities.";
    return nullptr;
  }

  return std::make_unique<PersistentPermanentEntity>(
      current_server_entity.GetId(), current_server_entity.GetVersion(),
      model_type, current_server_entity.GetName(),
      current_server_entity.GetParentId(),
      syncer::ModelTypeToRootTag(model_type), client_entity.specifics());
}

PersistentPermanentEntity::PersistentPermanentEntity(
    const string& id,
    int64_t version,
    const ModelType& model_type,
    const string& name,
    const string& parent_id,
    const string& server_defined_unique_tag,
    const sync_pb::EntitySpecifics& specifics)
    : LoopbackServerEntity(id, model_type, version, name),
      server_defined_unique_tag_(server_defined_unique_tag),
      parent_id_(parent_id) {
  SetSpecifics(specifics);
}

bool PersistentPermanentEntity::RequiresParentId() const {
  return true;
}

string PersistentPermanentEntity::GetParentId() const {
  return parent_id_;
}

void PersistentPermanentEntity::SerializeAsProto(
    sync_pb::SyncEntity* proto) const {
  LoopbackServerEntity::SerializeBaseProtoFields(proto);
  proto->set_server_defined_unique_tag(server_defined_unique_tag_);
}

bool PersistentPermanentEntity::IsFolder() const {
  return true;
}

bool PersistentPermanentEntity::IsPermanent() const {
  return true;
}

sync_pb::LoopbackServerEntity_Type
PersistentPermanentEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_PERMANENT;
}

}  // namespace syncer
