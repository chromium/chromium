// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/collaboration_metadata.h"

#include <cstddef>
#include <ostream>
#include <string>

#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace syncer {

CollaborationMetadata::CollaborationMetadata(
    const CollaborationMetadata& other) = default;
CollaborationMetadata& CollaborationMetadata::operator=(
    const CollaborationMetadata& other) = default;
CollaborationMetadata::CollaborationMetadata(CollaborationMetadata&& other) =
    default;
CollaborationMetadata& CollaborationMetadata::operator=(
    CollaborationMetadata&& other) = default;
CollaborationMetadata::~CollaborationMetadata() = default;

// static
CollaborationMetadata CollaborationMetadata::FromRemoteProto(
    const sync_pb::SyncEntity::CollaborationMetadata& remote_proto) {
  return CollaborationMetadata(
      /*created_by=*/GaiaId(
          remote_proto.creation_attribution().obfuscated_gaia_id()),
      /*last_updated_by=*/
      GaiaId(remote_proto.last_update_attribution().obfuscated_gaia_id()),
      CollaborationId(remote_proto.collaboration_id()));
}

// static
CollaborationMetadata CollaborationMetadata::FromLocalProto(
    const sync_pb::EntityMetadata::CollaborationMetadata& local_proto) {
  return CollaborationMetadata(
      /*created_by=*/GaiaId(
          local_proto.creation_attribution().obfuscated_gaia_id()),
      /*last_updated_by=*/
      GaiaId(local_proto.last_update_attribution().obfuscated_gaia_id()),
      CollaborationId(local_proto.collaboration_id()));
}

// static
CollaborationMetadata CollaborationMetadata::ForLocalChange(
    const GaiaId& changed_by,
    const CollaborationId& collaboration_id) {
  return CollaborationMetadata(
      /*created_by=*/changed_by,
      /*last_updated_by=*/changed_by, collaboration_id);
}

sync_pb::SyncEntity::CollaborationMetadata
CollaborationMetadata::ToRemoteProto() const {
  sync_pb::SyncEntity::CollaborationMetadata remote_proto;
  if (!created_by_.empty()) {
    remote_proto.mutable_creation_attribution()->set_obfuscated_gaia_id(
        created_by_.ToString());
  }
  if (!last_updated_by_.empty()) {
    remote_proto.mutable_last_update_attribution()->set_obfuscated_gaia_id(
        last_updated_by_.ToString());
  }
  remote_proto.set_collaboration_id(collaboration_id_.value());
  return remote_proto;
}

sync_pb::EntityMetadata::CollaborationMetadata
CollaborationMetadata::ToLocalProto() const {
  sync_pb::EntityMetadata::CollaborationMetadata local_proto;
  if (!created_by_.empty()) {
    local_proto.mutable_creation_attribution()->set_obfuscated_gaia_id(
        created_by_.ToString());
  }
  if (!last_updated_by_.empty()) {
    local_proto.mutable_last_update_attribution()->set_obfuscated_gaia_id(
        last_updated_by_.ToString());
  }
  local_proto.set_collaboration_id(collaboration_id_.value());
  return local_proto;
}

size_t CollaborationMetadata::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(created_by_.ToString()) +
         EstimateMemoryUsage(last_updated_by_.ToString()) +
         EstimateMemoryUsage(collaboration_id_.value());
}

CollaborationMetadata::CollaborationMetadata(GaiaId created_by,
                                             GaiaId last_updated_by,
                                             CollaborationId collaboration_id)
    : created_by_(std::move(created_by)),
      last_updated_by_(std::move(last_updated_by)),
      collaboration_id_(std::move(collaboration_id)) {}

void PrintTo(const CollaborationMetadata& collaboration_metadata,
             std::ostream* os) {
  *os << "{ collaboration_id: '"
      << collaboration_metadata.collaboration_id().value() << "', created_by: '"
      << collaboration_metadata.created_by() << "', last_updated_by: '"
      << collaboration_metadata.last_updated_by() << "'}";
}

}  // namespace syncer
