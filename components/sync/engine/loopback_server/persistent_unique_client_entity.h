// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_UNIQUE_CLIENT_ENTITY_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_UNIQUE_CLIENT_ENTITY_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

// An entity that is unique per client account.
class PersistentUniqueClientEntity : public LoopbackServerEntity {
 public:
  PersistentUniqueClientEntity(const std::string& id,
                               syncer::DataType data_type,
                               int64_t version,
                               const std::string& non_unique_name,
                               const std::string& client_tag_hash,
                               const sync_pb::EntitySpecifics& specifics,
                               int64_t creation_time,
                               int64_t last_modified_time,
                               const std::string& collaboration_id);

  ~PersistentUniqueClientEntity() override;

  // Factory function for creating a PersistentUniqueClientEntity.
  static std::unique_ptr<LoopbackServerEntity> CreateFromEntity(
      const sync_pb::SyncEntity& client_entity);

  // Factory function for creating a PersistentUniqueClientEntity for use in the
  // FakeServer injection API.
  static std::unique_ptr<LoopbackServerEntity> CreateFromSpecificsForTesting(
      const std::string& non_unique_name,
      const std::string& client_tag,
      const sync_pb::EntitySpecifics& entity_specifics,
      int64_t creation_time,
      int64_t last_modified_time);

  static std::unique_ptr<LoopbackServerEntity>
  CreateFromSharedSpecificsForTesting(
      const std::string& non_unique_name,
      const std::string& client_tag,
      const sync_pb::EntitySpecifics& entity_specifics,
      int64_t creation_time,
      int64_t last_modified_time,
      const std::string& collaboration_id);

  // LoopbackServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  sync_pb::LoopbackServerEntity_Type GetLoopbackServerEntityType()
      const override;

 private:
  // These member values have equivalent fields in SyncEntity.
  const std::string client_tag_hash_;
  const int64_t creation_time_;
  const int64_t last_modified_time_;
  const std::string collaboration_id_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_UNIQUE_CLIENT_ENTITY_H_
