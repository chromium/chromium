// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_COLLABORATION_METADATA_H_
#define COMPONENTS_SYNC_PROTOCOL_COLLABORATION_METADATA_H_

#include <iosfwd>
#include <string>

#include "components/sync/base/collaboration_id.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace syncer {

// Metadata associated with a shared Sync entity.
class CollaborationMetadata {
 public:
  CollaborationMetadata(const CollaborationMetadata& other);
  CollaborationMetadata& operator=(const CollaborationMetadata& other);
  CollaborationMetadata(CollaborationMetadata&& other);
  CollaborationMetadata& operator=(CollaborationMetadata&& other);
  ~CollaborationMetadata();

  static CollaborationMetadata FromRemoteProto(
      const sync_pb::SyncEntity::CollaborationMetadata& remote_metadata);
  static CollaborationMetadata FromLocalProto(
      const sync_pb::EntityMetadata::CollaborationMetadata& local_metadata);

  // Creates a CollaborationMetadata for a local change (either creation or
  // update). For a new entity, `changed_by` will be used for both `created_by`
  // and `last_updated_by`.
  static CollaborationMetadata ForLocalChange(
      const GaiaId& changed_by,
      const CollaborationId& collaboration_id);

  // The account that created the entity (may be empty).
  const GaiaId& created_by() const { return created_by_; }

  // The account that last updated the entity (may be empty).
  const GaiaId& last_updated_by() const { return last_updated_by_; }

  // The collaboration ID of the entity.
  const CollaborationId& collaboration_id() const { return collaboration_id_; }

  sync_pb::SyncEntity::CollaborationMetadata ToRemoteProto() const;
  sync_pb::EntityMetadata::CollaborationMetadata ToLocalProto() const;

  size_t EstimateMemoryUsage() const;

 private:
  CollaborationMetadata(GaiaId created_by,
                        GaiaId last_updated_by,
                        CollaborationId collaboration_id);

  GaiaId created_by_;
  GaiaId last_updated_by_;
  CollaborationId collaboration_id_;
};

// gMock printer helper.
void PrintTo(const CollaborationMetadata& collaboration_metadata,
             std::ostream* os);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_COLLABORATION_METADATA_H_
