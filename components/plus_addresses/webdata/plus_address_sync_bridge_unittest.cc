// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/plus_address_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

class PlusAddressSyncBridgeTest : public testing::Test {
 public:
  PlusAddressSyncBridgeTest()
      : db_backend_(base::MakeRefCounted<WebDatabaseBackend>(
            base::FilePath(WebDatabase::kInMemoryPath),
            /*delegate=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault())) {
    db_backend_->AddTable(std::make_unique<PlusAddressTable>());
    db_backend_->InitDatabase();
    RecreateBridge();
  }

  void RecreateBridge() {
    bridge_ = std::make_unique<PlusAddressSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), db_backend_);
  }

  PlusAddressSyncBridge& bridge() { return *bridge_; }

  PlusAddressTable& table() {
    return *PlusAddressTable::FromWebDatabase(db_backend_->database());
  }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<WebDatabaseBackend> db_backend_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<PlusAddressSyncBridge> bridge_;
};

// Tests that during the initial sync, when no metadata is stored yet,
// `ModelReadyToSync()` is called.
TEST_F(PlusAddressSyncBridgeTest, ModelReadyToSync_InitialSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync);
  RecreateBridge();
}

TEST_F(PlusAddressSyncBridgeTest, ModelReadyToSync_ExistingMetadata) {
  // Simulate that some metadata is stored.
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState::INITIAL_SYNC_DONE);
  ASSERT_TRUE(
      table().UpdateModelTypeState(syncer::PLUS_ADDRESS, model_type_state));

  // Expect that `ModelReadyToSync()` is called with the stored metadata when
  // the bridge is created.
  EXPECT_CALL(mock_processor(), ModelReadyToSync(syncer::MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::IsEmpty())));
  RecreateBridge();
}

TEST_F(PlusAddressSyncBridgeTest, IsEntityDataValid) {
  syncer::EntityData entity;
  sync_pb::PlusAddressSpecifics* specifics =
      entity.specifics.mutable_plus_address();
  // Missing a profile ID.
  EXPECT_FALSE(bridge().IsEntityDataValid(entity));
  specifics->set_profile_id(123);
  EXPECT_TRUE(bridge().IsEntityDataValid(entity));
}

TEST_F(PlusAddressSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_plus_address()->set_profile_id(123);
  EXPECT_EQ(bridge().GetStorageKey(entity), "123");
}

TEST_F(PlusAddressSyncBridgeTest, GetAllDataForDebugging) {
  const PlusProfile profile1 = test::GetPlusProfile();
  const PlusProfile profile2 = test::GetPlusProfile2();
  ASSERT_TRUE(table().AddPlusProfile(profile1));
  ASSERT_TRUE(table().AddPlusProfile(profile2));

  base::test::TestFuture<std::unique_ptr<syncer::DataBatch>> future;
  bridge().GetAllDataForDebugging(future.GetCallback());
  const std::unique_ptr<syncer::DataBatch>& batch = future.Get();
  std::vector<PlusProfile> profiles_from_batch;
  while (batch->HasNext()) {
    profiles_from_batch.push_back(
        PlusProfileFromEntityData(*batch->Next().second));
  }
  EXPECT_THAT(profiles_from_batch,
              testing::UnorderedElementsAre(profile1, profile2));
}

}  // namespace

}  // namespace plus_addresses
