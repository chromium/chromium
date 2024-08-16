// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/in_memory_metadata_change_list.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::StrictMock;

MATCHER_P(EqualsProto, expected, "") {
  return arg.SerializeAsString() == expected.SerializeAsString();
}

class MockMetadataChangeList : public MetadataChangeList {
 public:
  MOCK_METHOD(void,
              UpdateDataTypeState,
              (const sync_pb::DataTypeState& data_type_state),
              (override));
  MOCK_METHOD(void, ClearDataTypeState, (), (override));
  MOCK_METHOD(void,
              UpdateMetadata,
              (const std::string& storage_key,
               const sync_pb::EntityMetadata& metadata),
              (override));
  MOCK_METHOD(void,
              ClearMetadata,
              (const std::string& storage_key),
              (override));
};

TEST(InMemoryMetadataChangeListTest, ShouldTransferNothingIfEmptyChangeList) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  // StrictMock verifies no calls are issued.
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldTransferUpdateDataTypeState) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  sync_pb::DataTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateDataTypeState(state);

  EXPECT_CALL(mock_change_list, UpdateDataTypeState(EqualsProto(state)));
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldTransferClearDataTypeState) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  cl.ClearDataTypeState();

  EXPECT_CALL(mock_change_list, ClearDataTypeState());
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldTransferUpdateMetadata) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash("some_hash");
  cl.UpdateMetadata("client_tag", metadata);

  EXPECT_CALL(mock_change_list,
              UpdateMetadata("client_tag", EqualsProto(metadata)));
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldTransferClearMetadata) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  cl.ClearMetadata("client_tag");

  EXPECT_CALL(mock_change_list, ClearMetadata("client_tag"));
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldTransferMultipleChanges) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  sync_pb::DataTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateDataTypeState(state);

  sync_pb::EntityMetadata metadata1;
  metadata1.set_client_tag_hash("some_hash1");
  cl.UpdateMetadata("client_tag1", metadata1);

  sync_pb::EntityMetadata metadata2;
  metadata2.set_client_tag_hash("some_hash2");
  cl.UpdateMetadata("client_tag2", metadata2);

  EXPECT_CALL(mock_change_list, UpdateDataTypeState(EqualsProto(state)));
  EXPECT_CALL(mock_change_list,
              UpdateMetadata("client_tag1", EqualsProto(metadata1)));
  EXPECT_CALL(mock_change_list,
              UpdateMetadata("client_tag2", EqualsProto(metadata2)));
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldTransferClearDespitePriorUpdates) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  // Updates that should be ignored due to the later clears.
  {
    sync_pb::DataTypeState state;
    state.set_encryption_key_name("ekn");
    cl.UpdateDataTypeState(state);

    sync_pb::EntityMetadata metadata;
    metadata.set_client_tag_hash("some_hash");
    cl.UpdateMetadata("client_tag", metadata);
  }

  cl.ClearDataTypeState();
  cl.ClearMetadata("client_tag");

  EXPECT_CALL(mock_change_list, ClearDataTypeState());
  EXPECT_CALL(mock_change_list, ClearMetadata("client_tag"));
  cl.TransferChangesTo(&mock_change_list);
}

TEST(InMemoryMetadataChangeListTest, ShouldDropMetadataChange) {
  StrictMock<MockMetadataChangeList> mock_change_list;
  InMemoryMetadataChangeList cl;

  sync_pb::DataTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateDataTypeState(state);

  sync_pb::EntityMetadata metadata1;
  metadata1.set_client_tag_hash("some_hash1");
  cl.UpdateMetadata("client_tag1", metadata1);

  sync_pb::EntityMetadata metadata2;
  metadata2.set_client_tag_hash("some_hash2");
  cl.UpdateMetadata("client_tag2", metadata2);

  // client_tag1 is not transferred because it was dropped.
  cl.DropMetadataChangeForStorageKey("client_tag1");

  EXPECT_CALL(mock_change_list, UpdateDataTypeState(EqualsProto(state)));
  EXPECT_CALL(mock_change_list,
              UpdateMetadata("client_tag2", EqualsProto(metadata2)));
  cl.TransferChangesTo(&mock_change_list);
}

}  // namespace
}  // namespace syncer
