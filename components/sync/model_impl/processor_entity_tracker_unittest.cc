// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/processor_entity_tracker.h"

#include <utility>

#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kKey[] = "key";
const char kHash[] = "hash";
const char kId[] = "id";
const char kName[] = "name";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& name,
                                           const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(name);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

std::unique_ptr<EntityData> GenerateEntityData(const std::string& hash,
                                               const std::string& name,
                                               const std::string& value) {
  std::unique_ptr<EntityData> entity_data(new EntityData());
  entity_data->client_tag_hash = hash;
  entity_data->specifics = GenerateSpecifics(name, value);
  entity_data->non_unique_name = name;
  return entity_data;
}

UpdateResponseData GenerateUpdate(const ProcessorEntityTracker& entity,
                                  const std::string& hash,
                                  const std::string& id,
                                  const std::string& name,
                                  const std::string& value,
                                  const base::Time& mtime,
                                  int64_t version) {
  std::unique_ptr<EntityData> data = GenerateEntityData(hash, name, value);
  data->id = id;
  data->modification_time = mtime;
  UpdateResponseData update;
  update.entity = data->PassToPtr();
  update.response_version = version;
  return update;
}

UpdateResponseData GenerateTombstone(const ProcessorEntityTracker& entity,
                                     const std::string& hash,
                                     const std::string& id,
                                     const std::string& name,
                                     const base::Time& mtime,
                                     int64_t version) {
  std::unique_ptr<EntityData> data = std::make_unique<EntityData>();
  data->client_tag_hash = hash;
  data->non_unique_name = name;
  data->id = id;
  data->modification_time = mtime;
  UpdateResponseData update;
  update.entity = data->PassToPtr();
  update.response_version = version;
  return update;
}

CommitResponseData GenerateAckData(const CommitRequestData& request,
                                   const std::string id,
                                   int64_t version) {
  CommitResponseData response;
  response.id = id;
  response.client_tag_hash = request.entity->client_tag_hash;
  response.sequence_number = request.sequence_number;
  response.response_version = version;
  response.specifics_hash = request.specifics_hash;
  return response;
}

}  // namespace

// Some simple sanity tests for the ProcessorEntityTracker.
//
// A lot of the more complicated sync logic is implemented in the
// ClientTagBasedModelTypeProcessor that owns the ProcessorEntityTracker.  We
// can't unit test it here.
//
// Instead, we focus on simple tests to make sure that variables are getting
// properly intialized and flags properly set.  Anything more complicated would
// be a redundant and incomplete version of the ClientTagBasedModelTypeProcessor
// tests.
class ProcessorEntityTrackerTest : public ::testing::Test {
 public:
  ProcessorEntityTrackerTest()
      : ctime_(base::Time::Now() - base::TimeDelta::FromSeconds(1)) {}

  std::unique_ptr<ProcessorEntityTracker> CreateNew() {
    return ProcessorEntityTracker::CreateNew(kKey, kHash, "", ctime_);
  }

  std::unique_ptr<ProcessorEntityTracker> CreateNewWithEmptyStorageKey() {
    return ProcessorEntityTracker::CreateNew("", kHash, "", ctime_);
  }

  std::unique_ptr<ProcessorEntityTracker> CreateSynced() {
    std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();
    entity->RecordAcceptedUpdate(
        GenerateUpdate(*entity, kHash, kId, kName, kValue1, ctime_, 1));
    DCHECK(!entity->IsUnsynced());
    return entity;
  }

  std::unique_ptr<ProcessorEntityTracker> RestoreFromMetadata(
      sync_pb::EntityMetadata* entity_metadata) {
    return ProcessorEntityTracker::CreateFromMetadata(kKey, entity_metadata);
  }

  const base::Time ctime_;
};

// Test the state of the default new tracker.
TEST_F(ProcessorEntityTrackerTest, DefaultTracker) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();

  EXPECT_EQ(kKey, entity->storage_key());
  EXPECT_EQ(kHash, entity->metadata().client_tag_hash());
  EXPECT_EQ("", entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(ctime_), entity->metadata().creation_time());
  EXPECT_EQ(0, entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test creating and commiting a new local item.
TEST_F(ProcessorEntityTrackerTest, NewLocalItem) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue1));

  EXPECT_EQ("", entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, entity->metadata().server_version());
  EXPECT_NE(0, entity->metadata().modification_time());
  EXPECT_FALSE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
  EXPECT_TRUE(entity->HasCommitData());

  EXPECT_EQ(kValue1, entity->commit_data()->specifics.preference().value());

  // Generate a commit request. The metadata should not change.
  const sync_pb::EntityMetadata metadata_v1 = entity->metadata();
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(metadata_v1.SerializeAsString(),
            entity->metadata().SerializeAsString());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
  EXPECT_TRUE(entity->HasCommitData());

  const EntityData& data = request.entity.value();
  EXPECT_EQ("", data.id);
  EXPECT_EQ(kHash, data.client_tag_hash);
  EXPECT_EQ(kName, data.non_unique_name);
  EXPECT_EQ(kValue1, data.specifics.preference().value());
  EXPECT_EQ(TimeToProtoTime(ctime_), TimeToProtoTime(data.creation_time));
  EXPECT_EQ(entity->metadata().modification_time(),
            TimeToProtoTime(data.modification_time));
  EXPECT_FALSE(data.is_deleted());
  EXPECT_EQ(1, request.sequence_number);
  EXPECT_EQ(kUncommittedVersion, request.base_version);
  EXPECT_EQ(entity->metadata().specifics_hash(), request.specifics_hash);

  // Ack the commit.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 1), false);

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_EQ(metadata_v1.creation_time(), entity->metadata().creation_time());
  EXPECT_EQ(metadata_v1.modification_time(),
            entity->metadata().modification_time());
  EXPECT_FALSE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->UpdateIsReflection(1));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test state for a newly synced server item.
TEST_F(ProcessorEntityTrackerTest, NewServerItem) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();

  const base::Time mtime = base::Time::Now();
  entity->RecordAcceptedUpdate(
      GenerateUpdate(*entity, kHash, kId, kName, kValue1, mtime, 10));

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(10, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(mtime), entity->metadata().modification_time());
  EXPECT_FALSE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->UpdateIsReflection(9));
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test creating tracker for new server item with empty storage key, applying
// update and updating storage key.
TEST_F(ProcessorEntityTrackerTest, NewServerItem_EmptyStorageKey) {
  std::unique_ptr<ProcessorEntityTracker> entity =
      CreateNewWithEmptyStorageKey();

  EXPECT_EQ("", entity->storage_key());

  const base::Time mtime = base::Time::Now();
  entity->RecordAcceptedUpdate(
      GenerateUpdate(*entity, kHash, kId, kName, kValue1, mtime, 10));
  entity->SetStorageKey(kKey);
  EXPECT_EQ(kKey, entity->storage_key());
}

// Test state for a tombstone received for a previously unknown item.
TEST_F(ProcessorEntityTrackerTest, NewServerTombstone) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();

  const base::Time mtime = base::Time::Now();
  entity->RecordAcceptedUpdate(
      GenerateTombstone(*entity, kHash, kId, kName, mtime, 1));

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(mtime), entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->UpdateIsReflection(1));
  EXPECT_FALSE(entity->UpdateIsReflection(2));
  EXPECT_FALSE(entity->HasCommitData());
}

// Apply a deletion update to a synced item.
TEST_F(ProcessorEntityTrackerTest, ServerTombstone) {
  // Start with a non-deleted state with version 1.
  std::unique_ptr<ProcessorEntityTracker> entity = CreateSynced();
  // A deletion update one version later.
  const base::Time mtime = base::Time::Now();
  entity->RecordAcceptedUpdate(
      GenerateTombstone(*entity, kHash, kId, kName, mtime, 2));

  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(mtime), entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->UpdateIsReflection(2));
  EXPECT_FALSE(entity->UpdateIsReflection(3));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test a local change of a synced item.
TEST_F(ProcessorEntityTrackerTest, LocalChange) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateSynced();
  const int64_t mtime_v0 = entity->metadata().modification_time();
  const std::string specifics_hash_v0 = entity->metadata().specifics_hash();

  // Make a local change with different specifics.
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue2));

  const int64_t mtime_v1 = entity->metadata().modification_time();
  const std::string specifics_hash_v1 = entity->metadata().specifics_hash();

  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_LT(mtime_v0, mtime_v1);
  EXPECT_NE(specifics_hash_v0, specifics_hash_v1);
  EXPECT_EQ(specifics_hash_v0, entity->metadata().base_specifics_hash());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->HasCommitData());

  // Make a commit.
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);

  EXPECT_EQ(kId, request.entity->id);
  EXPECT_FALSE(entity->RequiresCommitRequest());

  // Ack the commit.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 2), false);

  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(mtime_v1, entity->metadata().modification_time());
  EXPECT_EQ(specifics_hash_v1, entity->metadata().specifics_hash());
  EXPECT_EQ("", entity->metadata().base_specifics_hash());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Test a local deletion of a synced item.
TEST_F(ProcessorEntityTrackerTest, LocalDeletion) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateSynced();
  const int64_t mtime = entity->metadata().modification_time();
  const std::string specifics_hash = entity->metadata().specifics_hash();

  // Make a local delete.
  entity->Delete();

  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_LT(mtime, entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_EQ(specifics_hash, entity->metadata().base_specifics_hash());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());

  // Generate a commit request. The metadata should not change.
  const sync_pb::EntityMetadata metadata_v1 = entity->metadata();
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(metadata_v1.SerializeAsString(),
            entity->metadata().SerializeAsString());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());

  const EntityData& data = request.entity.value();
  EXPECT_EQ(kId, data.id);
  EXPECT_EQ(kHash, data.client_tag_hash);
  EXPECT_EQ("", data.non_unique_name);
  EXPECT_EQ(TimeToProtoTime(ctime_), TimeToProtoTime(data.creation_time));
  EXPECT_EQ(entity->metadata().modification_time(),
            TimeToProtoTime(data.modification_time));
  EXPECT_TRUE(data.is_deleted());
  EXPECT_EQ(1, request.sequence_number);
  EXPECT_EQ(1, request.base_version);
  EXPECT_EQ(entity->metadata().specifics_hash(), request.specifics_hash);

  // Ack the deletion.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 2), false);

  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(metadata_v1.modification_time(),
            entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Test that hashes and sequence numbers are handled correctly for the "commit
// commit, ack ack" case.
TEST_F(ProcessorEntityTrackerTest, LocalChangesInterleaved) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateSynced();
  const std::string specifics_hash_v0 = entity->metadata().specifics_hash();

  // Make the first change.
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue2));
  const std::string specifics_hash_v1 = entity->metadata().specifics_hash();

  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_NE(specifics_hash_v0, specifics_hash_v1);
  EXPECT_EQ(specifics_hash_v0, entity->metadata().base_specifics_hash());

  // Request the first commit.
  CommitRequestData request_v1;
  entity->InitializeCommitRequestData(&request_v1);

  // Make the second change.
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue3));
  const std::string specifics_hash_v2 = entity->metadata().specifics_hash();

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_NE(specifics_hash_v1, specifics_hash_v2);
  EXPECT_EQ(specifics_hash_v0, entity->metadata().base_specifics_hash());

  // Request the second commit.
  CommitRequestData request_v2;
  entity->InitializeCommitRequestData(&request_v2);

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->HasCommitData());

  // Ack the first commit.
  entity->ReceiveCommitResponse(GenerateAckData(request_v1, kId, 2), false);

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(specifics_hash_v2, entity->metadata().specifics_hash());
  EXPECT_EQ(specifics_hash_v1, entity->metadata().base_specifics_hash());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->HasCommitData());

  // Ack the second commit.
  entity->ReceiveCommitResponse(GenerateAckData(request_v2, kId, 3), false);

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(2, entity->metadata().acked_sequence_number());
  EXPECT_EQ(3, entity->metadata().server_version());
  EXPECT_EQ(specifics_hash_v2, entity->metadata().specifics_hash());
  EXPECT_EQ("", entity->metadata().base_specifics_hash());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Tests that updating entity id with commit response while next local change is
// pending correctly updates that change's id and version.
TEST_F(ProcessorEntityTrackerTest, NewLocalChangeUpdatedId) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();
  // Create new local change. Make sure initial id is empty.
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue1));

  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_TRUE(request.entity->id.empty());

  // Before receiving commit response make local modification to the entity.
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue2));
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 1), false);

  // Receiving commit response with valid id should update
  // ProcessorEntityTracker. Consecutive commit requests should include updated
  // id.
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(kId, request.entity->id);
  EXPECT_EQ(1, request.base_version);
}

// Tests that entity restored after restart accepts specifics that don't match
// the ones passed originally to MakeLocalChange.
TEST_F(ProcessorEntityTrackerTest, RestoredLocalChangeWithUpdatedSpecifics) {
  // Create new entity and preserver its metadata.
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue1));
  sync_pb::EntityMetadata entity_metadata = entity->metadata();

  // Restore entity from metadata and emulate bridge passing different specifics
  // to SetCommitData.
  entity = RestoreFromMetadata(&entity_metadata);
  auto entity_data = GenerateEntityData(kHash, kName, kValue2);
  entity->SetCommitData(entity_data.get());

  // No verification is necessary. SetCommitData shouldn't DCHECK.
}

// Tests the scenario where a local creation conflicts with a remote deletion,
// where usually (and in this test) local wins. In this case, the remote update
// should be ignored but the server IDs should be updated.
TEST_F(ProcessorEntityTrackerTest, LocalCreationConflictsWithServerTombstone) {
  std::unique_ptr<ProcessorEntityTracker> entity = CreateNew();
  entity->MakeLocalChange(GenerateEntityData(kHash, kName, kValue1));

  ASSERT_TRUE(entity->IsUnsynced());
  ASSERT_TRUE(entity->RequiresCommitRequest());
  ASSERT_FALSE(entity->RequiresCommitData());
  ASSERT_TRUE(entity->HasCommitData());
  ASSERT_FALSE(entity->metadata().is_deleted());
  ASSERT_TRUE(entity->metadata().server_id().empty());

  {
    // Local creation should use a temporary server ID (which in this tracker
    // involves an empty string).
    CommitRequestData request;
    entity->InitializeCommitRequestData(&request);
    EXPECT_TRUE(request.entity->id.empty());
  }

  // Before anything gets committed, we receive a remote tombstone, but local
  // would usually win so the remote update is ignored.
  entity->RecordIgnoredUpdate(
      GenerateTombstone(*entity, kHash, kId, kName, base::Time::Now(), 2));

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->HasCommitData());
  EXPECT_FALSE(entity->metadata().is_deleted());

  // Generate a commit request. The server ID should have been reused from the
  // otherwise ignored update.
  const sync_pb::EntityMetadata metadata_v1 = entity->metadata();
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(kId, request.entity->id);
}

}  // namespace syncer
