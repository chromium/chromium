// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_BOOKMARK_ENTITY_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_BOOKMARK_ENTITY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace sync_pb {
class SyncEntity;
class EntitySpecifics;
enum LoopbackServerEntity_Type : int;
}  // namespace sync_pb

namespace syncer {

// A bookmark version of LoopbackServerEntity. This type represents entities
// that are non-deleted, client-created, and not unique per client account.
class PersistentBookmarkEntity : public LoopbackServerEntity {
 public:
  ~PersistentBookmarkEntity() override;

  // Factory function for PersistentBookmarkEntity. This factory should be used
  // only for the first time that a specific bookmark is seen by the server.
  static std::unique_ptr<LoopbackServerEntity> CreateNew(
      const sync_pb::SyncEntity& client_entity,
      const std::string& parent_id,
      const std::string& originator_cache_guid);

  // Factory function for PersistentBookmarkEntity. The server's current entity
  // for this ID, |current_server_entity|, is passed here because the client
  // does not always send the complete entity over the wire. This requires
  // copying of some of the existing entity when creating a new entity.
  static std::unique_ptr<LoopbackServerEntity> CreateUpdatedVersion(
      const sync_pb::SyncEntity& client_entity,
      const LoopbackServerEntity& current_server_entity,
      const std::string& parent_id,
      const std::string& originator_cache_guid);

  // Factory function for PersistentBookmarkEntity used when de-serializing the
  // information stored in the persistent storage.
  static std::unique_ptr<LoopbackServerEntity> CreateFromEntity(
      const sync_pb::SyncEntity& client_entity);

  PersistentBookmarkEntity(const std::string& id,
                           int64_t version,
                           const std::string& name,
                           const std::string& originator_cache_guid,
                           const std::string& originator_client_item_id,
                           const std::string& client_tag_hash,
                           const sync_pb::UniquePosition& unique_position,
                           const sync_pb::EntitySpecifics& specifics,
                           bool is_folder,
                           const std::string& parent_id,
                           int64_t creation_time,
                           int64_t last_modified_time);

  void SetParentId(const std::string& parent_id);

  // LoopbackServerEntity implementation.
  bool RequiresParentId() const override;
  std::string GetParentId() const override;
  void SerializeAsProto(sync_pb::SyncEntity* proto) const override;
  bool IsFolder() const override;
  sync_pb::LoopbackServerEntity_Type GetLoopbackServerEntityType()
      const override;

 private:
  // All member values have equivalent fields in SyncEntity.
  const std::string originator_cache_guid_;
  const std::string originator_client_item_id_;
  // Whether a client tag hash was provided upon *creation*, or an empty string
  // otherwise. Takes precedence over the two fields above when exposing the
  // entity in the protocol.
  const std::string client_tag_hash_;
  const bool is_folder_;
  sync_pb::UniquePosition unique_position_;
  std::string parent_id_;
  int64_t creation_time_;
  int64_t last_modified_time_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_PERSISTENT_BOOKMARK_ENTITY_H_
