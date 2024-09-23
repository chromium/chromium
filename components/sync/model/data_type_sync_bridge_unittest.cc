// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_sync_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/stub_data_type_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::_;
using testing::Return;

class DataTypeSyncBridgeTest : public ::testing::Test {
 public:
  DataTypeSyncBridgeTest()
      : bridge_(mock_processor_.CreateForwardingProcessor()) {}
  ~DataTypeSyncBridgeTest() override = default;

  StubDataTypeSyncBridge* bridge() { return &bridge_; }
  MockDataTypeLocalChangeProcessor* processor() { return &mock_processor_; }

 private:
  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  StubDataTypeSyncBridge bridge_;
};

// ResolveConflicts should return kUseRemote unless the remote data is deleted.
TEST_F(DataTypeSyncBridgeTest, DefaultConflictResolution) {
  EntityData local_data;
  EntityData remote_data;

  // There is no deleted/deleted case because that's not a conflict.

  local_data.specifics.mutable_preference()->set_value("value");
  EXPECT_FALSE(local_data.is_deleted());
  EXPECT_TRUE(remote_data.is_deleted());
  EXPECT_EQ(
      ConflictResolution::kUseLocal,
      bridge()->ResolveConflict(/*storage_key=*/std::string(), remote_data));

  remote_data.specifics.mutable_preference()->set_value("value");
  EXPECT_FALSE(local_data.is_deleted());
  EXPECT_FALSE(remote_data.is_deleted());
  EXPECT_EQ(
      ConflictResolution::kUseRemote,
      bridge()->ResolveConflict(/*storage_key=*/std::string(), remote_data));

  local_data.specifics.clear_preference();
  EXPECT_TRUE(local_data.is_deleted());
  EXPECT_FALSE(remote_data.is_deleted());
  EXPECT_EQ(
      ConflictResolution::kUseRemote,
      bridge()->ResolveConflict(/*storage_key=*/std::string(), remote_data));
}

}  // namespace
}  // namespace syncer
