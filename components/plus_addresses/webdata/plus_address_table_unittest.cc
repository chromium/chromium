// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_table.h"

#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

// Matches a std::pair<std::string, std::unique_ptr<some-proto>> against a given
// (std::string key, (some-proto) value) pair, comparing protos as strings.
MATCHER_P2(KeyAndProto, key, value, "") {
  return arg.first == key &&
         arg.second->SerializeAsString() == value.SerializeAsString();
}

class PlusAddressTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestDB")));
  }

  base::ScopedTempDir temp_dir_;
  PlusAddressTable table_;
  WebDatabase db_;
};

TEST_F(PlusAddressTableTest, GetPlusProfiles) {
  const PlusProfile profile1 =
      test::CreatePlusProfile(/*use_full_domain=*/true);
  const PlusProfile profile2 =
      test::CreatePlusProfile2(/*use_full_domain=*/true);
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(profile1));
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(profile2));
  EXPECT_THAT(table_.GetPlusProfiles(),
              testing::UnorderedElementsAre(profile1, profile2));
}

TEST_F(PlusAddressTableTest, GetPlusProfileForId) {
  const PlusProfile profile1 =
      test::CreatePlusProfile(/*use_full_domain=*/true);
  const PlusProfile profile2 =
      test::CreatePlusProfile2(/*use_full_domain=*/true);
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(profile1));
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(profile2));
  EXPECT_EQ(table_.GetPlusProfileForId(profile1.profile_id), profile1);
  EXPECT_EQ(table_.GetPlusProfileForId(profile2.profile_id), profile2);
  EXPECT_EQ(table_.GetPlusProfileForId("invalid_id"), std::nullopt);
}

TEST_F(PlusAddressTableTest, AddOrUpdatePlusProfile) {
  PlusProfile profile1 = test::CreatePlusProfile(/*use_full_domain=*/true);
  PlusProfile profile2 = test::CreatePlusProfile2(/*use_full_domain=*/true);
  // Add `profile1`.
  EXPECT_TRUE(table_.AddOrUpdatePlusProfile(profile1));
  EXPECT_THAT(table_.GetPlusProfiles(),
              testing::UnorderedElementsAre(profile1));
  // Update `profile1`.
  profile1.plus_address = "new-" + profile1.plus_address;
  EXPECT_TRUE(table_.AddOrUpdatePlusProfile(profile1));
  EXPECT_THAT(table_.GetPlusProfiles(),
              testing::UnorderedElementsAre(profile1));
  // Add `profile2`.
  EXPECT_TRUE(table_.AddOrUpdatePlusProfile(profile2));
  EXPECT_THAT(table_.GetPlusProfiles(),
              testing::UnorderedElementsAre(profile1, profile2));
}

TEST_F(PlusAddressTableTest, RemovePlusProfile) {
  const PlusProfile profile1 =
      test::CreatePlusProfile(/*use_full_domain=*/true);
  const PlusProfile profile2 =
      test::CreatePlusProfile2(/*use_full_domain=*/true);
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(profile1));
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(profile2));
  EXPECT_TRUE(table_.RemovePlusProfile(profile1.profile_id));
  EXPECT_THAT(table_.GetPlusProfiles(),
              testing::UnorderedElementsAre(profile2));
  // Removing a non-existing `profile_id` shouldn't be considered a failure.
  EXPECT_TRUE(table_.RemovePlusProfile(profile1.profile_id));
}

TEST_F(PlusAddressTableTest, ClearPlusProfiles) {
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(
      test::CreatePlusProfile(/*use_full_domain=*/true)));
  ASSERT_TRUE(table_.AddOrUpdatePlusProfile(
      test::CreatePlusProfile2(/*use_full_domain=*/true)));
  EXPECT_TRUE(table_.ClearPlusProfiles());
  EXPECT_THAT(table_.GetPlusProfiles(), testing::IsEmpty());
}

// Tests that when no sync metadata is persisted, `GetAllSyncMetadata()` returns
// the default model type state without any entity metadata.
TEST_F(PlusAddressTableTest, SyncMetadataStore_NoData) {
  syncer::MetadataBatch metadata;
  EXPECT_TRUE(table_.GetAllSyncMetadata(syncer::PLUS_ADDRESS, metadata));
  EXPECT_EQ(metadata.GetModelTypeState().SerializeAsString(),
            sync_pb::ModelTypeState().SerializeAsString());
  EXPECT_THAT(metadata.TakeAllMetadata(), testing::IsEmpty());
}

// Tests adding and updating the sync model type state.
TEST_F(PlusAddressTableTest, SyncMetadataStore_ModifyModelTypeState) {
  // Add
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  EXPECT_TRUE(
      table_.UpdateModelTypeState(syncer::PLUS_ADDRESS, model_type_state));
  syncer::MetadataBatch metadata;
  EXPECT_TRUE(table_.GetAllSyncMetadata(syncer::PLUS_ADDRESS, metadata));
  EXPECT_EQ(metadata.GetModelTypeState().SerializeAsString(),
            model_type_state.SerializeAsString());

  // Update
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState::INITIAL_SYNC_DONE);
  EXPECT_TRUE(
      table_.UpdateModelTypeState(syncer::PLUS_ADDRESS, model_type_state));
  EXPECT_TRUE(table_.GetAllSyncMetadata(syncer::PLUS_ADDRESS, metadata));
  EXPECT_EQ(metadata.GetModelTypeState().SerializeAsString(),
            model_type_state.SerializeAsString());
}

// Tests adding and updating sync entity metadata.
TEST_F(PlusAddressTableTest, SyncMetadataStore_ModifyEntityMetadata) {
  // Add two entities.
  sync_pb::EntityMetadata entity1;
  entity1.set_creation_time(123);
  EXPECT_TRUE(
      table_.UpdateEntityMetadata(syncer::PLUS_ADDRESS, "key1", entity1));
  sync_pb::EntityMetadata entity2;
  entity2.set_creation_time(234);
  EXPECT_TRUE(
      table_.UpdateEntityMetadata(syncer::PLUS_ADDRESS, "key2", entity2));
  syncer::MetadataBatch metadata;
  EXPECT_TRUE(table_.GetAllSyncMetadata(syncer::PLUS_ADDRESS, metadata));
  EXPECT_THAT(metadata.TakeAllMetadata(),
              testing::UnorderedElementsAre(KeyAndProto("key1", entity1),
                                            KeyAndProto("key2", entity2)));

  // Update `entity1`.
  entity1.set_modification_time(543);
  EXPECT_TRUE(
      table_.UpdateEntityMetadata(syncer::PLUS_ADDRESS, "key1", entity1));
  EXPECT_TRUE(table_.GetAllSyncMetadata(syncer::PLUS_ADDRESS, metadata));
  EXPECT_THAT(metadata.TakeAllMetadata(),
              testing::UnorderedElementsAre(KeyAndProto("key1", entity1),
                                            KeyAndProto("key2", entity2)));
}

// Tests clearing the sync model type state + entity metadata.
TEST_F(PlusAddressTableTest, SyncMetadataStore_Clear) {
  // Add some dummy data.
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState::INITIAL_SYNC_STATE_UNSPECIFIED);
  ASSERT_TRUE(
      table_.UpdateModelTypeState(syncer::PLUS_ADDRESS, model_type_state));
  sync_pb::EntityMetadata entity;
  entity.set_creation_time(123);
  ASSERT_TRUE(table_.UpdateEntityMetadata(syncer::PLUS_ADDRESS, "key", entity));

  // Clear model type state and entity metadata.
  EXPECT_TRUE(table_.ClearModelTypeState(syncer::PLUS_ADDRESS));
  EXPECT_TRUE(table_.ClearEntityMetadata(syncer::PLUS_ADDRESS, "key"));

  // Expect that no data remains.
  syncer::MetadataBatch metadata;
  EXPECT_TRUE(table_.GetAllSyncMetadata(syncer::PLUS_ADDRESS, metadata));
  EXPECT_EQ(metadata.GetModelTypeState().SerializeAsString(),
            sync_pb::ModelTypeState().SerializeAsString());
  EXPECT_THAT(metadata.TakeAllMetadata(), testing::IsEmpty());
}

}  // namespace

}  // namespace plus_addresses
