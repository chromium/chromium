// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/collaboration_metadata.h"

#include "base/test/protobuf_matchers.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace syncer {
namespace {

using base::test::EqualsProto;

constexpr char kCollaborationId[] = "collaboration";
constexpr char kCreatorId[] = "creator_id";
constexpr char kUpdaterId[] = "updater_id";

TEST(CollaborationMetadataTest, SerializeAndDeserializeLocalProto) {
  sync_pb::EntityMetadata::CollaborationMetadata local_proto;
  local_proto.set_collaboration_id(kCollaborationId);
  local_proto.mutable_creation_attribution()->set_obfuscated_gaia_id(
      kCreatorId);
  local_proto.mutable_last_update_attribution()->set_obfuscated_gaia_id(
      kUpdaterId);
  EXPECT_THAT(CollaborationMetadata::FromLocalProto(local_proto).ToLocalProto(),
              EqualsProto(local_proto));
}

TEST(CollaborationMetadataTest, SerializeAndDeserializeRemoteProto) {
  sync_pb::SyncEntity::CollaborationMetadata remote_proto;
  remote_proto.set_collaboration_id(kCollaborationId);
  remote_proto.mutable_creation_attribution()->set_obfuscated_gaia_id(
      kCreatorId);
  remote_proto.mutable_last_update_attribution()->set_obfuscated_gaia_id(
      kUpdaterId);
  EXPECT_THAT(
      CollaborationMetadata::FromRemoteProto(remote_proto).ToRemoteProto(),
      EqualsProto(remote_proto));
}

TEST(CollaborationMetadataTest, SerializeLocalChangeToProtos) {
  // Creator and updater are the same for the local change.
  sync_pb::EntityMetadata::CollaborationMetadata expected_local_proto;
  expected_local_proto.set_collaboration_id(kCollaborationId);
  expected_local_proto.mutable_creation_attribution()->set_obfuscated_gaia_id(
      kCreatorId);
  expected_local_proto.mutable_last_update_attribution()
      ->set_obfuscated_gaia_id(kCreatorId);

  sync_pb::SyncEntity::CollaborationMetadata expected_remote_proto;
  expected_remote_proto.set_collaboration_id(kCollaborationId);
  expected_remote_proto.mutable_creation_attribution()->set_obfuscated_gaia_id(
      kCreatorId);
  expected_remote_proto.mutable_last_update_attribution()
      ->set_obfuscated_gaia_id(kCreatorId);

  CollaborationMetadata collaboration_metadata =
      CollaborationMetadata::ForLocalChange(GaiaId(kCreatorId),
                                            CollaborationId(kCollaborationId));
  EXPECT_THAT(collaboration_metadata.ToLocalProto(),
              EqualsProto(expected_local_proto));
  EXPECT_THAT(collaboration_metadata.ToRemoteProto(),
              EqualsProto(expected_remote_proto));
}

}  // namespace
}  // namespace syncer
