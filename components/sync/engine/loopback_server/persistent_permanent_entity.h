// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_PERMANENT_ENTITY_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_PERMANENT_ENTITY_H_

#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
enum LoopbackServerEntity_Type : int;
}  // namespace sync_pb

namespace syncer {

// A server-created, permanent entity.
class PersistentPermanentEntity : public LoopbackServerEntity {
 public:
  PersistentPermanentEntity(const std::string& id,
                            int64_t version,
                            const syncer::DataType& data_type,
                            const std::string& name,
                            const std::string& parent_id,
                            const std::string& server_defined_unique_tag,
                            const sync_pb::EntitySpecifics& entity_specifics);

  ~PersistentPermanentEntity() override;

  // Factory function for PersistentPermanentEntity. |server_tag| should be a
  // globally unique identifier.
  static std::unique_ptr<LoopbackServerEntity> CreateNew(
      const syncer::DataType& data_type,
      const std::string& server_tag,
      const std::string& name,
      const std::string& parent_server_tag);

  // Factory function for a top level PersistentPermanentEntity. Top level means
  // that the entity's parent is the root entity (no PersistentPermanentEntity
  // exists for root).
  static std::unique_ptr<LoopbackServerEntity> CreateTopLevel(
      const syncer::DataType& data_type);

  // Factory function for creating an updated version of a
  // PersistentPermanentEntity. This function should only be called for the
  // Nigori entity.
  static std::unique_ptr<LoopbackServerEntity> CreateUpdatedNigoriEntity(
      const sync_pb::SyncEntity& client_entity,
      const LoopbackServerEntity& current_server_entity);

  // LoopbackServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsFolder() const override;
  bool IsPermanent() const override;
  sync_pb::LoopbackServerEntity_Type GetLoopbackServerEntityType()
      const override;

 private:
  // All member values have equivalent fields in SyncEntity.
  std::string server_defined_unique_tag_;
  std::string parent_id_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_PERMANENT_ENTITY_H_
