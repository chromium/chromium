// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_PERSISTENT_TOMBSTONE_ENTITY_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_PERSISTENT_TOMBSTONE_ENTITY_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/sync.pb.h"

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
      const std::string& client_tag_hash);

  PersistentTombstoneEntity(const std::string& id,
                            int64_t version,
                            const syncer::ModelType& model_type,
                            const std::string& client_tag_hash);

  // The tag hash for this entity.
  const std::string client_tag_hash_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_LOOPBACK_SERVER_PERSISTENT_TOMBSTONE_ENTITY_H_
