// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/processor_entity_metadata.h"

#include <memory>

#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

sync_pb::EntityMetadata CreateValidMetadataProto() {
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash("hash");
  metadata.set_creation_time(TimeToProtoTime(base::Time::Now()));
  metadata.set_sequence_number(1);
  metadata.set_acked_sequence_number(0);
  return metadata;
}

TEST(ProcessorEntityMetadataTest, ShouldBeValidForValidMetadata) {
  EXPECT_TRUE(ProcessorEntityMetadata::IsValid(CreateValidMetadataProto()));
}

TEST(ProcessorEntityMetadataTest, ShouldBeInvalidIfClientTagHashIsEmpty) {
  sync_pb::EntityMetadata metadata = CreateValidMetadataProto();
  metadata.clear_client_tag_hash();
  EXPECT_FALSE(ProcessorEntityMetadata::IsValid(metadata));
}

TEST(ProcessorEntityMetadataTest, ShouldBeInvalidIfCreationTimeIsMissing) {
  sync_pb::EntityMetadata metadata = CreateValidMetadataProto();
  metadata.clear_creation_time();
  EXPECT_FALSE(ProcessorEntityMetadata::IsValid(metadata));
}

TEST(ProcessorEntityMetadataTest,
     ShouldBeInvalidIfSequenceNumberIsLessThanAcked) {
  sync_pb::EntityMetadata metadata = CreateValidMetadataProto();
  metadata.set_sequence_number(0);
  metadata.set_acked_sequence_number(1);
  EXPECT_FALSE(ProcessorEntityMetadata::IsValid(metadata));
}

TEST(ProcessorEntityMetadataTest,
     ShouldBeUnsyncedAfterIncrementSequenceNumber) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), "server_id", base::Time::Now());
  ASSERT_FALSE(metadata.IsUnsynced());

  metadata.IncrementSequenceNumber();
  EXPECT_TRUE(metadata.IsUnsynced());

  // Test serialization.
  std::unique_ptr<ProcessorEntityMetadata> metadata_restored =
      ProcessorEntityMetadata::FromProto(metadata.proto());
  ASSERT_NE(metadata_restored, nullptr);
  EXPECT_TRUE(metadata_restored->IsUnsynced());
}

TEST(ProcessorEntityMetadataTest, ShouldNotBeUnsyncedAfterCommitResponse) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), "server_id", base::Time::Now());
  metadata.IncrementSequenceNumber();
  ASSERT_TRUE(metadata.IsUnsynced());

  CommitResponseData response;
  response.id = "server_id";
  response.client_tag_hash = ClientTagHash::FromHashed("hash");
  response.sequence_number = 1;
  response.response_version = 1;
  metadata.RecordCommitResponse(response);
  EXPECT_FALSE(metadata.IsUnsynced());

  // Test serialization.
  std::unique_ptr<ProcessorEntityMetadata> metadata_restored =
      ProcessorEntityMetadata::FromProto(metadata.proto());
  ASSERT_NE(metadata_restored, nullptr);
  EXPECT_FALSE(metadata_restored->IsUnsynced());
}

TEST(ProcessorEntityMetadataTest,
     ShouldBeUnsyncedLocalCreationAfterLocalUpdate) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), /*server_id=*/"", base::Time::Now());

  ASSERT_FALSE(metadata.IsUnsyncedLocalCreation());

  metadata.IncrementSequenceNumber();
  EXPECT_TRUE(metadata.IsUnsyncedLocalCreation());

  // Test serialization.
  std::unique_ptr<ProcessorEntityMetadata> metadata_restored =
      ProcessorEntityMetadata::FromProto(metadata.proto());
  ASSERT_NE(metadata_restored, nullptr);
  EXPECT_TRUE(metadata_restored->IsUnsyncedLocalCreation());
}

TEST(ProcessorEntityMetadataTest,
     ShouldNotBeUnsyncedLocalCreationAfterCommitResponse) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), /*server_id=*/"", base::Time::Now());
  metadata.IncrementSequenceNumber();
  ASSERT_TRUE(metadata.IsUnsyncedLocalCreation());

  // Simulate commit response.
  CommitResponseData response;
  response.id = "server_id";
  response.client_tag_hash = ClientTagHash::FromHashed("hash");
  response.sequence_number = 1;
  response.response_version = 12345;
  metadata.RecordCommitResponse(response);

  EXPECT_FALSE(metadata.IsUnsyncedLocalCreation());

  // Test serialization.
  std::unique_ptr<ProcessorEntityMetadata> metadata_restored =
      ProcessorEntityMetadata::FromProto(metadata.proto());
  ASSERT_NE(metadata_restored, nullptr);
  EXPECT_FALSE(metadata_restored->IsUnsyncedLocalCreation());
}

TEST(ProcessorEntityMetadataTest,
     ShouldUpdateBaseSpecificsHashOnFirstIncrement) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), "server_id", base::Time::Now());

  UpdateResponseData update;
  update.entity.id = "server_id";
  update.entity.specifics.mutable_preference()->set_name("name");
  update.response_version = 1;
  metadata.RecordAcceptedRemoteUpdate(update, /*trimmed_specifics=*/{},
                                      /*unique_position=*/std::nullopt);
  CommitResponseData response1;
  response1.id = "server_id";
  response1.sequence_number = metadata.proto().sequence_number();
  response1.response_version = 1;
  response1.specifics_hash = metadata.proto().specifics_hash();
  metadata.RecordCommitResponse(response1);

  const std::string initial_hash = metadata.proto().specifics_hash();
  ASSERT_FALSE(initial_hash.empty());

  // First local update (becomes unsynced, base_specifics_hash set to
  // initial_hash).
  EntityData local_data;
  local_data.specifics.mutable_preference()->set_name("new_name");
  metadata.RecordLocalUpdate(local_data, /*trimmed_specifics=*/{},
                             /*unique_position=*/std::nullopt);

  EXPECT_EQ(metadata.proto().base_specifics_hash(), initial_hash);
}

TEST(ProcessorEntityMetadataTest,
     ShouldNotUpdateBaseSpecificsHashOnSubsequentIncrements) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), "server_id", base::Time::Now());

  UpdateResponseData update;
  update.entity.id = "server_id";
  update.entity.specifics.mutable_preference()->set_name("name1");
  update.response_version = 1;
  metadata.RecordAcceptedRemoteUpdate(update, /*trimmed_specifics=*/{},
                                      /*unique_position=*/std::nullopt);

  CommitResponseData response2;
  response2.id = "server_id";
  response2.sequence_number = metadata.proto().sequence_number();
  response2.response_version = 1;
  response2.specifics_hash = metadata.proto().specifics_hash();
  metadata.RecordCommitResponse(response2);

  const std::string initial_hash = metadata.proto().specifics_hash();
  ASSERT_FALSE(initial_hash.empty());

  // First local update (becomes unsynced).
  EntityData local_data1;
  local_data1.specifics.mutable_preference()->set_name("name2");
  metadata.RecordLocalUpdate(local_data1, /*trimmed_specifics=*/{},
                             /*unique_position=*/std::nullopt);
  ASSERT_EQ(metadata.proto().base_specifics_hash(), initial_hash);

  // Second local update (already unsynced).
  EntityData local_data2;
  local_data2.specifics.mutable_preference()->set_name("name3");
  metadata.RecordLocalUpdate(local_data2, /*trimmed_specifics=*/{},
                             /*unique_position=*/std::nullopt);
  EXPECT_EQ(metadata.proto().base_specifics_hash(), initial_hash);
}

TEST(ProcessorEntityMetadataTest, ShouldRecordLocalDeletion) {
  ProcessorEntityMetadata metadata = ProcessorEntityMetadata::CreateNew(
      ClientTagHash::FromHashed("hash"), "server_id", base::Time::Now());
  ASSERT_FALSE(metadata.IsDeleted());

  metadata.RecordLocalDeletion(DeletionOrigin::Unspecified());
  EXPECT_TRUE(metadata.IsDeleted());

  // Test serialization.
  std::unique_ptr<ProcessorEntityMetadata> metadata_restored =
      ProcessorEntityMetadata::FromProto(metadata.proto());
  ASSERT_NE(metadata_restored, nullptr);
  EXPECT_TRUE(metadata_restored->IsDeleted());
}

}  // namespace

}  // namespace syncer
