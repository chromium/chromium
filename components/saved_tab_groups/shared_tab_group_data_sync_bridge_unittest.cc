// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/shared_tab_group_data_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;

namespace tab_groups {
namespace {

syncer::EntityData MakeTabGroup(const std::string& title,
                                sync_pb::SharedTabGroup::Color color) {
  syncer::EntityData entity_data;
  auto* shared_tab_group_data_specifics =
      entity_data.specifics.mutable_shared_tab_group_data();
  shared_tab_group_data_specifics->set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  auto* tab_group = shared_tab_group_data_specifics->mutable_tab_group();
  tab_group->set_title(title);
  tab_group->set_color(color);
  return entity_data;
}

class SharedTabGroupDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupDataSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void InitializeBridge() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        &saved_tab_group_model_,
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor());
    task_environment_.RunUntilIdle();
  }

  SharedTabGroupDataSyncBridge* bridge() { return bridge_.get(); }
  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &processor_;
  }

 private:
  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  SavedTabGroupModel saved_tab_group_model_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<SharedTabGroupDataSyncBridge> bridge_;
};

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnClientTag) {
  InitializeBridge();
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()
                   ->GetClientTag(MakeTabGroup("test title",
                                               sync_pb::SharedTabGroup::GREEN))
                   .empty());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCallModelReadyToSync) {
  EXPECT_CALL(*mock_processor(), ModelReadyToSync).WillOnce(Invoke([]() {}));

  // This already invokes RunUntilIdle, so the call above is expected to happen.
  InitializeBridge();
}

}  // namespace
}  // namespace tab_groups
