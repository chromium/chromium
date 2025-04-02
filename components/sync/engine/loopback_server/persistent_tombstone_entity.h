// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_TOMBSTONE_ENTITY_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_TOMBSTONE_ENTITY_H_

#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace sync_pb {
class SyncEntity;
enum LoopbackServerEntity_Type : int;
}  // namespace sync_pb

namespace syncer {

// A Sync entity that represents a deleted item.
class PersistentTombstoneEntity : public LoopbackServerEntity {
 public:
  ~PersistentTombstoneEntity() override;

  // Factory function for PersistentTombstoneEntity.
  static std::unique_ptr<LoopbackServerEntity> CreateFromEntity(
      const sync_pb::SyncEntity& id);

  static std::unique_ptr<LoopbackServerEntity> CreateNew(
      const std::string& id,
      const std::string& client_tag_hash);

  static std::unique_ptr<LoopbackServerEntity> CreateNewShared(
      const std::string& id,
      const std::string& client_tag_hash,
      const sync_pb::SyncEntity::CollaborationMetadata& collaboration_metadata);

  // LoopbackServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsDeleted() const override;
  sync_pb::LoopbackServerEntity_Type GetLoopbackServerEntityType()
      const override;

 private:
  static std::unique_ptr<LoopbackServerEntity> CreateNewInternal(
      const std::string& id,
      int64_t version,
      const std::string& client_tag_hash,
      const sync_pb::SyncEntity::CollaborationMetadata& collaboration_metadata);

  PersistentTombstoneEntity(
      const std::string& id,
      int64_t version,
      const syncer::DataType& data_type,
      const std::string& client_tag_hash,
      const sync_pb::SyncEntity::CollaborationMetadata& collaboration_metadata);

  // The tag hash for this entity.
  const std::string client_tag_hash_;

  // Collaboration metadata for this entity for shared data types.
  const sync_pb::SyncEntity::CollaborationMetadata collaboration_metadata_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_TOMBSTONE_ENTITY_H_
