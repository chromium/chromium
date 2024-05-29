// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace plus_addresses {

namespace {

syncer::EntityData CreateSettingEntity(
    const std::string& name,
    absl::variant<bool, std::string, int64_t> value) {
  syncer::EntityData entity;
  sync_pb::PlusAddressSettingSpecifics* specifics =
      entity.specifics.mutable_plus_address_setting();
  specifics->set_name(name);
  absl::visit(
      base::Overloaded{
          [&](bool value) { specifics->set_bool_value(value); },
          [&](const std::string& value) { specifics->set_string_value(value); },
          [&](int64_t value) { specifics->set_int_value(value); }},
      value);
  return entity;
}

class PlusAddressSettingSyncBridgeTest : public testing::Test {
 public:
  PlusAddressSettingSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    RecreateBridge();
  }

  void RecreateBridge() {
    bridge_ = std::make_unique<PlusAddressSettingSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
            store_.get()));
    // Even though the test uses an in-memory store, it still posts tasks. Wait
    // for initialisation to complete.
    task_environment_.RunUntilIdle();
  }

  PlusAddressSettingSyncBridge& bridge() { return *bridge_; }

  syncer::ModelTypeStore& store() { return *store_; }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<PlusAddressSettingSyncBridge> bridge_;
};

TEST_F(PlusAddressSettingSyncBridgeTest, ModelReadyToSync_InitialSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync);
  RecreateBridge();
}

TEST_F(PlusAddressSettingSyncBridgeTest, ModelReadyToSync_ExistingMetadata) {
  // Simulate that some metadata is stored.
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState::INITIAL_SYNC_DONE);
  auto write_batch = store().CreateWriteBatch();
  write_batch->GetMetadataChangeList()->UpdateModelTypeState(model_type_state);
  base::test::TestFuture<const std::optional<syncer::ModelError>&> write_result;
  store().CommitWriteBatch(std::move(write_batch), write_result.GetCallback());
  ASSERT_FALSE(write_result.Get());

  // Expect that `ModelReadyToSync()` is called with the stored metadata when
  // the bridge is created.
  EXPECT_CALL(mock_processor(), ModelReadyToSync(syncer::MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::IsEmpty())));
  RecreateBridge();
}

TEST_F(PlusAddressSettingSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity = CreateSettingEntity("name", "value");
  EXPECT_EQ(bridge().GetStorageKey(entity), "name");
  // `GetClientTag()` is implemented using `GetStorageKey()`.
  EXPECT_EQ(bridge().GetClientTag(entity), "name");
}

}  // namespace

}  // namespace plus_addresses
