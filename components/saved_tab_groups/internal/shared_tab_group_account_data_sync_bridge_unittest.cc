// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_account_data_sync_bridge.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

namespace {

using testing::Invoke;

sync_pb::SharedTabGroupAccountDataSpecifics MakeTabGroupAccountSpecifics(
    base::Time last_seen_timestamp) {
  sync_pb::SharedTabGroupAccountDataSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_collaboration_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  sync_pb::SharedTabDetails* tab_group_details =
      specifics.mutable_shared_tab_details();
  tab_group_details->set_shared_tab_group_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  tab_group_details->set_last_seen_timestamp_windows_epoch(
      last_seen_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

syncer::EntityData CreateEntityData(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics,
    base::Time creation_time = base::Time::Now()) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_account_data() = specifics;
  entity_data.name = specifics.guid();
  entity_data.creation_time = creation_time;
  return entity_data;
}

class SharedTabGroupAccountDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupAccountDataSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    feature_list_.InitAndEnableFeature(syncer::kSyncSharedTabGroupAccountData);
  }

  // Creates the bridges and initializes the model. Returns true when succeeds.
  void InitializeBridgeAndModel() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(processor_, ModelReadyToSync).WillOnce(Invoke([&]() {
      quit_closure.Run();
    }));

    bridge_ = std::make_unique<SharedTabGroupAccountDataSyncBridge>(
        std::make_unique<SyncDataTypeConfiguration>(
            processor_.CreateForwardingProcessor(),
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                store_.get())));

    run_loop.Run();

    ASSERT_TRUE(bridge().IsInitialized());
  }

  SharedTabGroupAccountDataSyncBridge& bridge() { return *bridge_; }
  syncer::DataTypeStore& store() { return *store_; }
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() {
    return processor_;
  }
  const testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() const {
    return processor_;
  }

 protected:
  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  std::unique_ptr<SharedTabGroupAccountDataSyncBridge> bridge_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SharedTabGroupAccountDataSyncBridgeTest, ShouldReturnClientTag) {
  InitializeBridgeAndModel();

  EXPECT_TRUE(bridge().SupportsGetClientTag());
  sync_pb::SharedTabGroupAccountDataSpecifics shared_tab_details =
      MakeTabGroupAccountSpecifics(base::Time::Now());
  EXPECT_EQ(
      bridge().GetClientTag(CreateEntityData(shared_tab_details)),
      shared_tab_details.guid() + "|" + shared_tab_details.collaboration_id());
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldAddDataAtIncrementalUpdate) {
  InitializeBridgeAndModel();

  // TODO(crbug.com/397767033): Add test for applying incremental
  // updates from sync.
}

TEST_F(SharedTabGroupAccountDataSyncBridgeTest,
       ShouldReloadDataOnBrowserRestart) {
  InitializeBridgeAndModel();

  // TODO(crbug.com/397767033): Add test for loading store data after
  // restart.
}

}  // namespace
}  // namespace tab_groups
